#include "pch.h"
#include "AccelerationStructureHandler.h"

#include "AccelerationStructureAllocator.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/BufferAllocator.h"
#include "Reflex/VK/Mesh/LoadMesh.h"
#include "Reflex/VK/Mesh/MeshHandler.h"

AccelerationStructureHandler::AccelerationStructureHandler(
	VulkanFramework& vulkanFramework,
	AccelerationStructureAllocator& accStructAllocator)
	: HandlerBase(vulkanFramework)
	, theirAccStructAllocator(accStructAllocator)
	, myGeometryStructures{}
{
	// ID EMPLACEMENT
	for (int i = 0; i < MaxNumInstanceStructures; ++i)
	{
		myFreeIDs.push(InstanceStructID(i));
	}

	// DESCRIPTOR POOL
	VkDescriptorPoolSize poolSize{};
	poolSize.descriptorCount = MaxNumInstanceStructures;
	poolSize.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = NULL;

	poolInfo.maxSets = MaxNumInstanceStructures;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	auto failure = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!failure && "failed creating descriptor pool");

	// INSTANCE STRUCTURES LAYOUT
	VkDescriptorSetLayoutBinding binding{};
	binding.descriptorCount = MaxNumInstanceStructures;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
	binding.binding = 0;
	binding.stageFlags =
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = NULL;

	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	failure = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &myInstanceStructDescriptorSetLayout);
	assert(!failure && "failed creating desc set layout");

	// INSTANCE STRUCTURES SET
	VkDescriptorSetAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.descriptorPool = myDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &myInstanceStructDescriptorSetLayout;

	failure = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &myInstanceStructDescriptorSet);
	assert(!failure && "failed allocating desc set");

	// FILL DEFAULT INSTANCE STRUCTS
	auto allocSub = theirAccStructAllocator.Start();
	auto [resultInstanceStruct, instanceStruct] = theirAccStructAllocator.RequestInstanceStructure(allocSub, {{}});
	assert(!resultInstanceStruct && "failed creating default instance structure");

	for (uint32_t i = 0; i < MaxNumInstanceStructures; ++i)
	{

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

		write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
		write.descriptorCount = 1;
		write.dstArrayElement = i;
		write.dstSet = myInstanceStructDescriptorSet;
		write.dstBinding = 0;

		VkWriteDescriptorSetAccelerationStructureNV accStructInfo;
		accStructInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
		accStructInfo.pNext = nullptr;
		accStructInfo.accelerationStructureCount = 1;
		accStructInfo.pAccelerationStructures = &instanceStruct;

		write.pNext = &accStructInfo;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

		myInstanceStructures[i] = {accStructInfo, instanceStruct};
	}
	
	// FILL DEFAULT GEO STRUCTS
	const auto rawMesh = LoadRawMesh("cube.dae", {});

	VkBuffer vBuffer, iBuffer;
	std::tie(failure, vBuffer) = theirAccStructAllocator.GetBufferAllocator().RequestVertexBuffer(allocSub, rawMesh.vertices, theirAccStructAllocator.GetOwners());
	assert(!failure && "failed allocating default vertex buffer");
	std::tie(failure, iBuffer) = theirAccStructAllocator.GetBufferAllocator().RequestIndexBuffer(allocSub, rawMesh.indices, theirAccStructAllocator.GetOwners());
	assert(!failure && "failed allocating default index buffer");

	MeshGeometry defaultGeo = {};
	defaultGeo.vertexBuffer = vBuffer;
	defaultGeo.indexBuffer = iBuffer;
	defaultGeo.numVertices = uint32_t(rawMesh.vertices.size());
	defaultGeo.numIndices = uint32_t(rawMesh.indices.size());

	const auto& meshes = { defaultGeo };
	std::tie(failure, myDefaultGeoStructure) = theirAccStructAllocator.RequestGeometryStructure(allocSub, { meshes });
	myGeometryStructures.fill(myDefaultGeoStructure);
	
	theirAccStructAllocator.Queue(std::move(allocSub));
}

AccelerationStructureHandler::~AccelerationStructureHandler()
{
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), myInstanceStructDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

VkResult
AccelerationStructureHandler::LoadGeometryStructure(
	MeshID					meshID,
	AllocationSubmission&	allocSub,
	const MeshGeometry&		mesh)
{
	const auto& meshes = { mesh };
	auto [failure, geoStruct] = theirAccStructAllocator.RequestGeometryStructure(allocSub, {meshes});
	if (failure)
	{
		return failure;
	}

	myGeometryStructures[int(meshID)] = geoStruct;

	return VK_SUCCESS;
}

void
AccelerationStructureHandler::UnloadGeometryStructure(
	MeshID meshID)
{
	if (myGeometryStructures[int(meshID)] == myDefaultGeoStructure
		|| BAD_ID(meshID))
	{
		return;
	}
	auto signal = std::make_shared<std::counting_semaphore<NumSwapchainImages>>(0);
	theirAccStructAllocator.QueueDestroy(myGeometryStructures[int(meshID)], signal);
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; swapchainIndex++)
	{
		myQueuedUnloadSignals[swapchainIndex].push(signal);
	}

	myGeometryStructures[int(meshID)] = myDefaultGeoStructure;
}

void
AccelerationStructureHandler::SignalUnload(
	int		swapchainIndex,
	VkFence fence)
{
	if (myQueuedUnloadSignals[swapchainIndex].empty())
	{
		return;
	}
	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), fence))
	{
	}
	
	shared_semaphore<NumSwapchainImages> semaphore;
	int count = 0;
	while (myQueuedUnloadSignals[swapchainIndex].try_pop(semaphore)
		&& count++ < 128)
	{
		semaphore->release();
	}
}

InstanceStructID
AccelerationStructureHandler::AddInstanceStructure(
		AllocationSubmission& allocSub,
		const RTInstances& instances)
{
	auto [failure, instanceStruct] = theirAccStructAllocator.RequestInstanceStructure(allocSub, instances);
	if (failure)
	{
		return InstanceStructID(INVALID_ID);
	}
	
	InstanceStructID id;
	bool success = myFreeIDs.try_pop(id);
	assert(success && "failed attaining id");

	myInstanceStructures[int(id)].structure = instanceStruct;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
	write.descriptorCount = 1;
	write.dstArrayElement = uint32_t(id);
	write.dstSet = myInstanceStructDescriptorSet;
	write.dstBinding = 0;

	
	myInstanceStructures[int(id)].info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
	myInstanceStructures[int(id)].info.pNext = nullptr;
	myInstanceStructures[int(id)].info.accelerationStructureCount = 1;
	myInstanceStructures[int(id)].info.pAccelerationStructures = &instanceStruct;

	write.pNext = &myInstanceStructures[int(id)].info;
	
	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return id;
}

void
AccelerationStructureHandler::UpdateInstanceStructure(
	InstanceStructID	id, 
	const RTInstances&	instances)
{
	theirAccStructAllocator.UpdateInstanceStructure(myInstanceStructures[int(id)].structure, instances);
}

VkDescriptorSetLayout
AccelerationStructureHandler::GetInstanceStructuresLayout() const
{
	return myInstanceStructDescriptorSetLayout;
}

VkDescriptorSet
AccelerationStructureHandler::GetInstanceStructuresSet() const
{
	return myInstanceStructDescriptorSet;
}

VkAccelerationStructureNV
AccelerationStructureHandler::GetGeometryStruct(MeshID id)
{
	return myGeometryStructures[int(id)];
}

void
AccelerationStructureHandler::BindInstanceStructures(
	VkCommandBuffer		commandBuffer, 
	VkPipelineLayout	pipelineLayout, 
	uint32_t			setIndex) const
{
	vkCmdBindDescriptorSets(commandBuffer,
							 VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
							 pipelineLayout,
							 setIndex,
							 1,
							 &myInstanceStructDescriptorSet,
							 0,
							 nullptr);
}
