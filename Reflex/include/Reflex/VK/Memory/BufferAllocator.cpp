#include "pch.h"
#include "BufferAllocator.h"

#include "ImmediateTransferrer.h"
#include "Reflex/VK/Geometry/Vertex2D.h"
#include "Reflex/VK/Geometry/Vertex3D.h"
#include "Reflex/VK/VulkanFramework.h"

BufferAllocator::~BufferAllocator()
{
	for (auto& [buffer, memory] : myAllocatedBuffers)
	{
		if (buffer)
		{
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		}
		else
		{
			assert(false && "how did you get here?");
		}
		if (memory)
		{
			vkFreeMemory(theirVulkanFramework.GetDevice(), memory, nullptr);
		}
	}
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestVertexBuffer(
	const Vertex3D*			firstVertex,
	uint32_t				numVertices,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners)
{
	auto [result, buffer, memory] = CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
												  firstVertex,
												  sizeof(Vertex3D) * numVertices,
												  firstOwner,
												  numOwners,
												  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
												  true
	);
	if (result)
	{
		if (buffer)
		{
			myAllocatedBuffers[buffer] = nullptr;
		}
		LOG("failed creating vertex buffer");
		return {result, buffer};
	}

	myAllocatedBuffers[buffer] = memory;
	myBufferSizes[buffer] = sizeof(Vertex3D) * numVertices;
	return {result, buffer};
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestVertexBuffer(
	const Vertex2D*			firstVertex, 
	uint32_t				numVertices, 
	const QueueFamilyIndex* firstOwner, 
	uint32_t				numOwners)
{
	auto [result, buffer, memory] = CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
												  firstVertex,
												  sizeof(Vertex2D) * numVertices,
												  firstOwner,
												  numOwners,
												  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
												  true
	);
	if (result)
	{
		if (buffer)
		{
			myAllocatedBuffers[buffer] = nullptr;
		}
		LOG("failed creating vertex buffer");
		return {result, buffer};
	}

	myAllocatedBuffers[buffer] = memory;
	myBufferSizes[buffer] = sizeof(Vertex2D) * numVertices;
	return {result, buffer};
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestIndexBuffer(
	const uint32_t*				firstIndex,
	uint32_t					numIndices,
	const QueueFamilyIndex*		firstOwner,
	uint32_t					numOwners)
{
	auto [result, buffer, memory] = CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
												  firstIndex,
												  sizeof(uint32_t) * numIndices,
												  firstOwner,
												  numOwners,
												  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
												  true
	);
	if (result)
	{
		if (buffer)
		{
			myAllocatedBuffers[buffer] = nullptr;
		}
		LOG("failed creating index buffer");
		return {result, buffer};
	}

	myAllocatedBuffers[buffer] = memory;
	myBufferSizes[buffer] = sizeof(uint32_t) * numIndices;
	return {result, buffer};
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestUniformBuffer(
	const void*				startData,
	size_t					size,
	const QueueFamilyIndex*	firstOwner,
	uint32_t				numOwners,
	VkMemoryPropertyFlags	memPropFlags)
{
	auto [result, buffer, memory] = CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
												  startData,
												  size,
												  firstOwner,
												  numOwners,
												  memPropFlags
	);
	if (result)
	{
		if (buffer)
		{
			myAllocatedBuffers[buffer] = nullptr;
		}
		LOG("failed creating uniform buffer");
		return {result, buffer};
	}

	myAllocatedBuffers[buffer] = memory;
	myBufferSizes[buffer] = size;
	return {result, buffer};
}

void
BufferAllocator::RequestBufferView(VkBuffer buffer)
{

}

void
BufferAllocator::UpdateBufferData(
	VkBuffer	buffer,	
	const void* data)
{
	std::scoped_lock<std::mutex> lock(myTransferMutex);

	vkCmdUpdateBuffer(myTransferBuffers[myTransferIndex], buffer, 0, myBufferSizes[buffer], data);

	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

	barrier.buffer = buffer;
	barrier.size = myBufferSizes[buffer];
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(myTransferBuffers[myTransferIndex],
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
						  VK_DEPENDENCY_DEVICE_GROUP_BIT,
						  0, nullptr,
						  1, &barrier,
						  0, nullptr);
}


std::tuple<VkResult, VkBuffer, VkDeviceMemory>
BufferAllocator::CreateBuffer(
	VkBufferUsageFlags		usage,
	const void*				data,
	size_t					size,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners,
	VkMemoryPropertyFlags	memPropFlags,
	bool					immediateTransfer)
{
	VkBuffer buffer{};
	VkDeviceMemory memory{};

	// BUFFER CREATION
	VkBufferCreateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.flags = NULL;

	bufferInfo.usage = usage;

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

	// MEM ALLOC
	auto [memReq, memTypeIndex] = GetMemReq(buffer, memPropFlags);
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
	// MEM STAGE
	if (data)
	{
		if (immediateTransfer)
		{
			auto [failure, stagedBuffer, stagedMem] = CreateStagingBuffer(data, size, firstOwner, numOwners, true);
			VkBufferCopy copy;
			copy.size = size;
			copy.srcOffset = 0;
			copy.dstOffset = 0;
			theirImmediateTransferrer.DoImmediateTransfer([&] (auto cmdBuffer)
			{
				vkCmdCopyBuffer(cmdBuffer, stagedBuffer, buffer, 1, &copy);
			});
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), stagedBuffer, nullptr);
			vkFreeMemory(theirVulkanFramework.GetDevice(), stagedMem, nullptr);
		}
		else
		{
			if (usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
			{
				void* mappedData;
				vkMapMemory(theirVulkanFramework.GetDevice(), memory, 0, size, NULL, &mappedData);
				memcpy(mappedData, data, size);
				vkUnmapMemory(theirVulkanFramework.GetDevice(), memory);
			}
			else if (usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT)
			{
				VkBufferCopy copy;
				copy.size = size;
				copy.srcOffset = 0;
				copy.dstOffset = 0;

				std::scoped_lock<std::mutex> lock(myTransferMutex);
				auto [resultStaged, stagedBuffer, stagedMemory] = CreateStagingBuffer(data, size, firstOwner, numOwners);
				vkCmdCopyBuffer(myTransferBuffers[myTransferIndex], stagedBuffer, buffer, 1, &copy);
			}
		}

	}
	return {VK_SUCCESS, buffer, memory};
}







