#include "pch.h"
#include "AllocatorBase.h"

#include "Reflex/VK/Debug/DebugUtils.h"
#include "Reflex/VK/VulkanFramework.h"

AllocatorBase::AllocatorBase(
	VulkanFramework&			vulkanFramework,
	class ImmediateTransferrer& immediateTransferrer,
	QueueFamilyIndex			transferFamilyIndex)
	: theirVulkanFramework(vulkanFramework)
	, theirImmediateTransferrer(immediateTransferrer)
{
	// INTI TRANSFER BUFFERS
	auto [result0, buffer0] = vulkanFramework.RequestCommandBuffer(transferFamilyIndex);
	auto [result1, buffer1] = vulkanFramework.RequestCommandBuffer(transferFamilyIndex);
	assert(!(result0 | result1) && "failed to create transfer buffers!");
	myTransferBuffers[0] = buffer0;
	myTransferBuffers[1] = buffer1;

	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	auto resultFence0 = vkCreateFence(vulkanFramework.GetDevice(), &fenceInfo, nullptr, &myTransferFences[0]);
	auto resultFence1 = vkCreateFence(vulkanFramework.GetDevice(), &fenceInfo, nullptr, &myTransferFences[1]);
	assert(!(resultFence0 | resultFence1) && "failed to crete fences");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;
	beginInfo.pInheritanceInfo = nullptr;

	auto resultBegin = vkBeginCommandBuffer(myTransferBuffers[myTransferIndex], &beginInfo);
	assert(!resultBegin && "failed to start recording");

	// ENUMERATE PHYSICAL DEVICE MEMORY
	myPhysicalDeviceMemProperties = theirVulkanFramework.GetPhysicalDeviceMemProps();

	for (MemTypeIndex typeIndex = 0; typeIndex < myPhysicalDeviceMemProperties.memoryTypeCount; typeIndex++)
	{
		constexpr int memPropFlagMax = 128;
		for (VkMemoryPropertyFlags a = 1; a <= memPropFlagMax; a = a << 1)
		{
			if (myPhysicalDeviceMemProperties.memoryTypes[typeIndex].propertyFlags & a)
			{
				myMemoryTypes[a].emplace_back(typeIndex);
			}
		}
		myMemoryTypes[myPhysicalDeviceMemProperties.memoryTypes[typeIndex].propertyFlags].emplace_back(typeIndex);
	}
}

AllocatorBase::~AllocatorBase()
{
	vkDestroyFence(theirVulkanFramework.GetDevice(), myTransferFences[0], nullptr);
	vkDestroyFence(theirVulkanFramework.GetDevice(), myTransferFences[1], nullptr);
	for (auto& [buffer, mem] : myStagingBuffers[0])
	{
		vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		vkFreeMemory(theirVulkanFramework.GetDevice(), mem, nullptr);
	}
	for (auto& [buffer, mem] : myStagingBuffers[1])
	{
		vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		vkFreeMemory(theirVulkanFramework.GetDevice(), mem, nullptr);
	}
}

std::tuple<VkCommandBuffer, VkFence>
AllocatorBase::AcquireNextTransferBuffer()
{
	std::scoped_lock<std::mutex> lock(myTransferMutex);

	auto result = vkEndCommandBuffer(myTransferBuffers[myTransferIndex]);
	assert(!result);

	// BEGIN RECORDING NEXT TRANSFER BUFFER
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;
	beginInfo.pInheritanceInfo = nullptr;

	while (auto stat = vkGetFenceStatus(theirVulkanFramework.GetDevice(), myTransferFences[!myTransferIndex]))
	{
	}
	result = vkBeginCommandBuffer(myTransferBuffers[!myTransferIndex], &beginInfo);
	assert(!result);

	// REMOVE OLD STAGING BUFFERS
	for (auto& [buffer, mem] : myStagingBuffers[!myTransferIndex])
	{
		vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		vkFreeMemory(theirVulkanFramework.GetDevice(), mem, nullptr);
	}
	myStagingBuffers[!myTransferIndex].clear();

	// WAIT, SWAP AND RETURN
	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myTransferFences[myTransferIndex]))
	{
	}
	myTransferIndex = !myTransferIndex;
	return {myTransferBuffers[!myTransferIndex], myTransferFences[!myTransferIndex]};
}

bool
AllocatorBase::IsExclusive(
	const QueueFamilyIndex* firstOwner, 
	uint32_t				numOwners)
{
	bool exclusive = numOwners == 1 || !firstOwner;

	for (uint32_t outer = 0; outer < numOwners; ++outer)
	{
		for (uint32_t inner = 0; inner < numOwners; ++inner)
		{
			if (inner == outer)
				continue;
			if (firstOwner[outer] == firstOwner[inner])
			{
				exclusive = true;
			}
		}
	}

	return exclusive;
}

std::tuple<VkMemoryRequirements, MemTypeIndex>
AllocatorBase::GetMemReq(
	VkBuffer				buffer, 
	VkMemoryPropertyFlags	memPropFlags)
{
	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(theirVulkanFramework.GetDevice(), buffer, &memReq);

	int chosenIndex = UINT_MAX;
	for (auto& index : myMemoryTypes[memPropFlags])
	{
		if ((1 << index) & memReq.memoryTypeBits)
		{
			chosenIndex = index;
			break;
		}
	}

	return {memReq, chosenIndex};
}

std::tuple<VkMemoryRequirements, MemTypeIndex>
AllocatorBase::GetMemReq(
	VkImage					image, 
	VkMemoryPropertyFlags	memPropFlags)
{
	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(theirVulkanFramework.GetDevice(), image, &memReq);

	MemTypeIndex chosenIndex = UINT_MAX;
	for (auto& index : myMemoryTypes[memPropFlags])
	{
		if ((1 << index) & memReq.memoryTypeBits)
		{
			chosenIndex = index;
			break;
		}
	}

	return {memReq, chosenIndex};
}



std::tuple<VkResult, VkBuffer, VkDeviceMemory>
AllocatorBase::CreateStagingBuffer(
	const void*				data,
	size_t					size,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners,
	bool					forImmediateUse)
{
	VkBuffer buffer{};
	VkDeviceMemory memory{};

	// BUFFER CREATION
	VkBufferCreateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.flags = NULL;

	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (IsExclusive(firstOwner, numOwners))
	{
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.pQueueFamilyIndices = nullptr;
		bufferInfo.queueFamilyIndexCount = 0;
	}
	else
	{
		bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferInfo.pQueueFamilyIndices = firstOwner;
		bufferInfo.queueFamilyIndexCount = numOwners;
	}

	bufferInfo.size = size;

	auto resultBuffer = vkCreateBuffer(theirVulkanFramework.GetDevice(), &bufferInfo, nullptr, &buffer);

	if (resultBuffer)
	{
		LOG("failed to create buffer");
		return {resultBuffer, buffer, memory};
	}

	DebugSetObjectName("Staging Buffer", buffer, VK_OBJECT_TYPE_BUFFER, theirVulkanFramework.GetDevice());

	// MEM ALLOC
	auto [memReq, memTypeIndex] = GetMemReq(buffer, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	if (memTypeIndex == UINT_MAX)
	{
		LOG("no appropriate memory type found");
		return {VK_ERROR_FEATURE_NOT_PRESENT, buffer, memory};
	}

	VkMemoryAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memTypeIndex;

	auto resultAlloc = vkAllocateMemory(theirVulkanFramework.GetDevice(), &allocInfo, nullptr, &memory);
	if (resultAlloc)
	{
		LOG("failed allocating memory");
		return {resultAlloc, buffer, memory};
	}

	// MEM BIND
	auto resultBind = vkBindBufferMemory(theirVulkanFramework.GetDevice(), buffer, memory, 0);
	if (resultBind)
	{
		LOG("failed to bind memory");
		return {resultBind, buffer, memory};
	}

	// MEM MAP
	void* mappedData;
	vkMapMemory(theirVulkanFramework.GetDevice(), memory, 0, size, NULL, &mappedData);
	memcpy(mappedData, data, size);
	vkUnmapMemory(theirVulkanFramework.GetDevice(), memory);

	if (!forImmediateUse)
	{
		myStagingBuffers[myTransferIndex].emplace_back(buffer, memory);
	}
	return {VK_SUCCESS, buffer, memory};
}
