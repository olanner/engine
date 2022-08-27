#include "pch.h"
#include "AccelerationStructureHandler.h"

#include "AccelerationStructureAllocator.h"
#include "RFVK/VulkanFramework.h"
#include "RFVK/Memory/BufferAllocator.h"
#include "RFVK/Mesh/LoadMesh.h"
#include "RFVK/Mesh/MeshHandler.h"

AccelerationStructureHandler::AccelerationStructureHandler(
	VulkanFramework& vulkanFramework,
	AccelerationStructureAllocator& accStructAllocator)
	: HandlerBase(vulkanFramework)
	, theirAccStructAllocator(accStructAllocator)
	, myGeometryStructures{}
	, myFreeGeoIDs(MaxNumMeshesLoaded)
	, myFreeInstanceIDs(MaxNumInstanceStructures)
{
	// DESCRIPTOR POOL
	VkDescriptorPoolSize poolSize = {};
	poolSize.descriptorCount = MaxNumInstanceStructures * NumSwapchainImages;
	poolSize.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = NULL;

	poolInfo.maxSets = NumSwapchainImages;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;

	auto failure = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!failure && "failed creating descriptor pool");

	// INSTANCE STRUCTURES LAYOUT
	VkDescriptorSetLayoutBinding binding = {};
	binding.descriptorCount = MaxNumInstanceStructures;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	binding.binding = 0;
	binding.stageFlags =
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = NULL;

	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	failure = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &myInstanceStructDescriptorSetLayout);
	assert(!failure && "failed creating desc set layout");

	// INSTANCE STRUCTURES SET
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.descriptorPool = myDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &myInstanceStructDescriptorSetLayout;

	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		failure = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &myInstanceStructDescriptorSets[swapchainIndex]);
		assert(!failure && "failed allocating desc set");
	}

	auto allocSubID = theirAccStructAllocator.Start();
	// FILL DEFAULT GEO STRUCTS
	const auto rawMesh = LoadRawMesh("cube.dae", {});

	VkBuffer vBuffer, iBuffer;
	std::tie(failure, vBuffer) = theirAccStructAllocator.GetBufferAllocator().RequestVertexBuffer(allocSubID, rawMesh.vertices, theirAccStructAllocator.GetOwners());
	assert(!failure && "failed allocating default vertex buffer");
	std::tie(failure, iBuffer) = theirAccStructAllocator.GetBufferAllocator().RequestIndexBuffer(allocSubID, rawMesh.indices, theirAccStructAllocator.GetOwners());
	assert(!failure && "failed allocating default index buffer");

	MeshGeometry defaultGeo = {};
	defaultGeo.vertexBuffer = vBuffer;
	defaultGeo.indexBuffer = iBuffer;
	defaultGeo.numVertices = uint32_t(rawMesh.vertices.size());
	defaultGeo.numIndices = uint32_t(rawMesh.indices.size());
	VkBufferDeviceAddressInfo addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = vBuffer;
	defaultGeo.vertexAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);
	addressInfo.buffer = iBuffer;
	defaultGeo.indexAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);

	const auto& meshes = { defaultGeo };
	std::tie(failure, myDefaultGeoStructure.structure) = theirAccStructAllocator.RequestGeometryStructure(allocSubID, { meshes });

	VkAccelerationStructureDeviceAddressInfoKHR accAddressInfo = {};
	accAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accAddressInfo.accelerationStructure = myDefaultGeoStructure.structure;
	myDefaultGeoStructure.address = vkGetAccelerationStructureDeviceAddress(theirVulkanFramework.GetDevice(), &accAddressInfo);
	
	myGeometryStructures.fill(myDefaultGeoStructure);
	
	// FILL DEFAULT INSTANCE STRUCTS
	VkAccelerationStructureInstanceKHR instance;
	instance.flags = NULL;
	instance.accelerationStructureReference = myDefaultGeoStructure.address;
	instance.instanceCustomIndex = 0;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.mask = 0xff;
	auto transform = glm::identity<Mat4rt>();
	instance.transform = *(VkTransformMatrixKHR*)&transform;
	
	auto [resultInstanceStruct, instanceStruct] = theirAccStructAllocator.RequestInstanceStructure(allocSubID, {instance});
	assert(!resultInstanceStruct && "failed creating default instance structure");

	for (uint32_t i = 0; i < MaxNumInstanceStructures; ++i)
	{
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

		write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		write.descriptorCount = 1;
		write.dstArrayElement = i;
		write.dstBinding = 0;

		VkWriteDescriptorSetAccelerationStructureKHR accStructInfo;
		accStructInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		accStructInfo.pNext = nullptr;
		accStructInfo.accelerationStructureCount = 1;
		accStructInfo.pAccelerationStructures = &instanceStruct;

		write.pNext = &accStructInfo;

		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			write.dstSet = myInstanceStructDescriptorSets[swapchainIndex];
			accStructInfo.pAccelerationStructures = &instanceStruct;
			myInstanceStructures[i].infos[swapchainIndex] = accStructInfo;
			myInstanceStructures[i].structures[swapchainIndex] = instanceStruct;
			
			vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
		}
	}
	
	theirAccStructAllocator.Queue(std::move(allocSubID));
}

AccelerationStructureHandler::~AccelerationStructureHandler()
{
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), myInstanceStructDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

GeoStructID AccelerationStructureHandler::AddGeometryStructure()
{
	return myFreeGeoIDs.FetchFreeID();
}

VkResult
AccelerationStructureHandler::LoadGeometryStructure(
	GeoStructID				geoStructID,
	AllocationSubmissionID	allocSubID,
	const MeshGeometry&		mesh)
{
	const auto& meshes = { mesh };
	auto [failure, geoStruct] = theirAccStructAllocator.RequestGeometryStructure(allocSubID, {meshes});
	if (failure)
	{
		return failure;
	}

	myGeometryStructures[int(geoStructID)].structure = geoStruct;
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	addressInfo.accelerationStructure = myGeometryStructures[int(geoStructID)].structure;
	myGeometryStructures[int(geoStructID)].address = vkGetAccelerationStructureDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);

	return VK_SUCCESS;
}

void
AccelerationStructureHandler::UnloadGeometryStructure(
	GeoStructID geoID)
{
	if (myGeometryStructures[int(geoID)].structure == myDefaultGeoStructure.structure
		|| BAD_ID(geoID))
	{
		return;
	}
	auto signal = std::make_shared<std::counting_semaphore<NumSwapchainImages>>(0);
	theirAccStructAllocator.QueueDestroy(myGeometryStructures[int(geoID)].structure, signal);
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; swapchainIndex++)
	{
		myQueuedUnloadSignals[swapchainIndex].push(signal);
	}

	myGeometryStructures[int(geoID)] = myDefaultGeoStructure;
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
		AllocationSubmissionID allocSubID,
		const RTInstances& instances)
{
	InstanceStructID id = myFreeInstanceIDs.FetchFreeID();
	if (BAD_ID(id))
	{
		return id;
	}
	auto& instanceStructure = myInstanceStructures[int(id)];
	
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		auto [failure, structure] = theirAccStructAllocator.RequestInstanceStructure(allocSubID, instances);
		if (failure)
		{
			return InstanceStructID(INVALID_ID);
		}
		instanceStructure.structures[swapchainIndex] = structure;
	}
	
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	write.descriptorCount = 1;
	write.dstArrayElement = uint32_t(id);
	write.dstBinding = 0;

	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		write.dstSet = myInstanceStructDescriptorSets[swapchainIndex];
		instanceStructure.infos[swapchainIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		instanceStructure.infos[swapchainIndex].pNext = nullptr;
		instanceStructure.infos[swapchainIndex].accelerationStructureCount = 1;
		instanceStructure.infos[swapchainIndex].pAccelerationStructures = &instanceStructure.structures[swapchainIndex];

		write.pNext = &instanceStructure.infos[swapchainIndex];

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}
	
	

	return id;
}

void
AccelerationStructureHandler::UpdateInstanceStructure(
	int					swapchainIndex,
	InstanceStructID	id, 
	const RTInstances&	instances)
{
	theirAccStructAllocator.UpdateInstanceStructure(myInstanceStructures[int(id)].structures[swapchainIndex], instances);
}

VkDescriptorSetLayout
AccelerationStructureHandler::GetInstanceStructuresLayout() const
{
	return myInstanceStructDescriptorSetLayout;
}

GeometryStructure
AccelerationStructureHandler::operator[](
	GeoStructID geoStructID)
{
	return myGeometryStructures[int(geoStructID)];
}

void
AccelerationStructureHandler::BindInstanceStructures(
	int					swapchainIndex,
	VkCommandBuffer		commandBuffer, 
	VkPipelineLayout	pipelineLayout, 
	uint32_t			setIndex) const
{
	vkCmdBindDescriptorSets(commandBuffer,
							 VK_PIPELINE_BIND_POINT_RAY_TRACING_NV,
							 pipelineLayout,
							 setIndex,
							 1,
							 &myInstanceStructDescriptorSets[swapchainIndex],
							 0,
							 nullptr);
}
