#include "pch.h"
#include "UniformHandler.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/BufferAllocator.h"

UniformHandler::UniformHandler(
	VulkanFramework&		vulkanFramework,
	BufferAllocator&		bufferAllocator,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirBufferAllocator(bufferAllocator)
{
	for (uint32_t i = 0; i < numOwners; ++i)
	{
		myOwners.emplace_back(firstOwner[i]);
	}

	for (uint32_t i = 0; i < MaxNumUniforms; ++i)
	{
		myFreeIDs.push(UniformID(i));
	}
}

UniformHandler::~UniformHandler()
{
	for (auto& [buffer, layout] : myUniformLayouts)
	{
		vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), layout, nullptr);
	}

	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

VkResult
UniformHandler::Init()
{
	VkDescriptorPoolCreateInfo info;
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.pNext = nullptr;
	info.flags = NULL;

	VkDescriptorPoolSize size;
	size.descriptorCount = 128;
	size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	info.pPoolSizes = &size;
	info.poolSizeCount = 1;
	info.maxSets = 128;

	auto resultPool = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &info, nullptr, &myDescriptorPool);
	VK_FALLTHROUGH(resultPool);



	return VK_SUCCESS;
}

UniformID
UniformHandler::RequestUniformBuffer(
	const void* startData,
	size_t		size)
{
	// BUFFER
	auto allocSub = theirBufferAllocator.Start();
	auto [resultUB, buffer] = theirBufferAllocator.RequestUniformBuffer(
		allocSub,
		startData,
		size,
		myOwners,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	theirBufferAllocator.Queue(std::move(allocSub));
	if (resultUB)
	{
		LOG("failed to get uniform buffer");
		return UniformID(INVALID_ID);
	}

	// LAYOUT
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding.descriptorCount = 1;
	binding.pImmutableSamplers = nullptr;
	binding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;


	VkDescriptorSetLayoutCreateInfo layoutInfo;
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = NULL;

	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	VkDescriptorSetLayout layout;
	auto resultLayout = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &layout);
	if (resultLayout)
	{
		LOG("failed to create desc set layout");
		return UniformID(INVALID_ID);
	}

	// SET
	VkDescriptorSetAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.descriptorPool = myDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet set;
	auto resultSet = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &set);
	if (resultSet)
	{
		LOG("failed to create set");
		return UniformID(INVALID_ID);
	}

	myUniformSets[buffer] = set;
	myUniformLayouts[buffer] = layout;
	
	// UPDATE DESCRIPTOR
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = size;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.dstSet = set;
	write.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	UniformID id;
	bool success = myFreeIDs.try_pop(id);
	assert(success && "failed attaining id");
	myUniforms[int(id)] = buffer;
	return id;
}

void
UniformHandler::UpdateUniformData(
	UniformID	id, 
	const void* data)
{
	if (BAD_ID(id))
	{
		return;
	}
	theirBufferAllocator.UpdateBufferData(myUniforms[int(id)], data);
}

std::tuple<VkDescriptorSetLayout, VkDescriptorSet>
	UniformHandler::operator[](UniformID id)
{
	if (BAD_ID(id))
	{
		return {nullptr, nullptr};
	}
	return {myUniformLayouts[myUniforms[int(id)]], myUniformSets[myUniforms[int(id)]]};
}

void
UniformHandler::BindUniform(
	UniformID			id,
	VkCommandBuffer		cmdBuffer,
	VkPipelineLayout	layout,
	uint32_t			setSlot,
	VkPipelineBindPoint bindPoint)
{
	vkCmdBindDescriptorSets(cmdBuffer,
							bindPoint,
							layout,
							setSlot,
							1,
							&myUniformSets[myUniforms[int(id)]],
							0,
							nullptr);
}
