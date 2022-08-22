#include "pch.h"
#include "AccelerationStructureHandler.h"

#include "AccelerationStructureAllocator.h"
#include "Reflex/VK/VulkanFramework.h"

AccelerationStructureHandler::AccelerationStructureHandler(
	VulkanFramework& vulkanFramework,
	AccelerationStructureAllocator& accStructAllocator)
	: theirVulkanFramework(vulkanFramework)
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
		myInstanceStructures[i] = instanceStruct;

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
	}
	theirAccStructAllocator.Queue(std::move(allocSub));
}

AccelerationStructureHandler::~AccelerationStructureHandler()
{
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), myInstanceStructDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

VkResult
AccelerationStructureHandler::AddGeometryStructure(
	AllocationSubmission&	allocSub,
	MeshID					ownerID,
	const MeshGeometry*		firstMesh, 
	uint32_t				numMeshes)
{
	auto [failure, geoStruct] = theirAccStructAllocator.RequestGeometryStructure(allocSub, firstMesh, numMeshes);
	if (failure)
	{
		return failure;
	}

	myGeometryStructures[int(ownerID)] = geoStruct;

	return VK_SUCCESS;
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
	myInstanceStructures[int(id)] = instanceStruct;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NV;
	write.descriptorCount = 1;
	write.dstArrayElement = uint32_t(id);
	write.dstSet = myInstanceStructDescriptorSet;
	write.dstBinding = 0;

	VkWriteDescriptorSetAccelerationStructureNV accStructInfo;
	accStructInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_NV;
	accStructInfo.pNext = nullptr;
	accStructInfo.accelerationStructureCount = 1;
	accStructInfo.pAccelerationStructures = &instanceStruct;

	write.pNext = &accStructInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return id;
}

void
AccelerationStructureHandler::UpdateInstanceStructure(
	InstanceStructID	id, 
	const RTInstances&	instances)
{
	theirAccStructAllocator.UpdateInstanceStructure(myInstanceStructures[int(id)], instances);
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
