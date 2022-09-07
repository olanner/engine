#include "pch.h"
#include "BufferAllocator.h"

#include "ImmediateTransferrer.h"
#include "RFVK/Geometry/Vertex2D.h"
#include "RFVK/Geometry/Vertex3D.h"
#include "RFVK/VulkanFramework.h"

BufferAllocator::~BufferAllocator()
{
	{
		AllocatedBuffer allocBuffer;
		while (myRequestedBuffersQueue.try_pop(allocBuffer))
		{
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocBuffer.buffer, nullptr);
			vkFreeMemory(theirVulkanFramework.GetDevice(), allocBuffer.memory, nullptr);
		}
	}
	for (auto&& [buffer, allocBuffer] : myAllocatedBuffers)
	{
		vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocBuffer.buffer, nullptr);
		vkFreeMemory(theirVulkanFramework.GetDevice(), allocBuffer.memory, nullptr);
	}
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestVertexBuffer(
	AllocationSubmissionID allocSubID,
	const std::vector<Vertex3D>& vertices, 
	const std::vector<QueueFamilyIndex>& owners)
{
	auto [result, buffer, memory] = 
		CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			vertices.data(),
			sizeof(Vertex3D) * vertices.size(),
			owners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (result)
	{
		if (buffer)
		{
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		}
		LOG("failed creating vertex buffer");
		return {result, nullptr};
	}

	myRequestedBuffersQueue.push({
	buffer,
	memory,
	sizeof(Vertex3D) * vertices.size()
		});
	
	return {result, buffer};
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestVertexBuffer(
	AllocationSubmissionID					allocSubID,
	const std::vector<struct Vertex2D>&		vertices,
	const std::vector<QueueFamilyIndex>&	owners)
{
	auto [result, buffer, memory] = 
		CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			vertices.data(),
			sizeof(Vertex2D) * vertices.size(),
			owners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (result)
	{
		if (buffer)
		{
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		}
		LOG("failed creating vertex buffer");
		return {result, nullptr};
	}

	myRequestedBuffersQueue.push({
		buffer,
		memory,
		sizeof(Vertex2D)* vertices.size()
		});
	
	return {result, buffer};
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestIndexBuffer(
	AllocationSubmissionID					allocSubID,
	const std::vector<uint32_t>&			indices,
	const std::vector<QueueFamilyIndex>&	owners)
{
	auto [result, buffer, memory] = 
		CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT 
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			indices.data(),
			sizeof(uint32_t) * indices.size(),
			owners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (result)
	{
		if (buffer)
		{
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		}
		LOG("failed creating index buffer");
		return {result, nullptr};
	}

	myRequestedBuffersQueue.push({
		buffer,
		memory,
		sizeof(uint32_t)* indices.size()
		});
	
	return {result, buffer};
}

std::tuple<VkResult, VkBuffer>
BufferAllocator::RequestUniformBuffer(
	AllocationSubmissionID					allocSubID,
	const void*								startData,
	size_t									size,
	const std::vector<QueueFamilyIndex>&	owners,
	VkMemoryPropertyFlags					memPropFlags)
{
	auto [result, buffer, memory] = CreateBuffer(
									allocSubID,
									VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									startData,
									size,
									owners,
									memPropFlags);
	if (result)
	{
		if (buffer)
		{
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), buffer, nullptr);
		}
		LOG("failed creating uniform buffer");
		return {result, nullptr};
	}

	myRequestedBuffersQueue.push({
		buffer,
		memory,
		size
		});
	return {result, buffer};
}

void
BufferAllocator::RequestBufferView(VkBuffer buffer)
{

}

void
BufferAllocator::UpdateBufferData(
	VkBuffer								buffer,	
	const void*								data,
	size_t									offset,
	size_t									size,
	const std::vector<QueueFamilyIndex>&	owners)
{
	assert(data && "invalid data to update buffer width");
	
	// TODO: maybe do this better lol
	auto allocSubID = theirAllocationSubmitter.StartAllocSubmission();
	auto& allocSub = theirAllocationSubmitter[allocSubID];
	const auto cmdBuffer = allocSub.Record();

	size = 
		size > myAllocatedBuffers[buffer].size || size == 0
		? myAllocatedBuffers[buffer].size
		: size;
	
	VkBufferCopy copy;
	copy.size = size;
	copy.srcOffset = 0;
	copy.dstOffset = offset;

	auto [resultStaged, stagedBuffer] = CreateStagingBuffer(data, size, owners.data(), owners.size());
	vkCmdCopyBuffer(allocSub.Record(), stagedBuffer.buffer, buffer, 1, &copy);

	allocSub.AddResourceBuffer(stagedBuffer.buffer, stagedBuffer.memory);
	
	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	
	barrier.buffer = buffer;
	barrier.size = size;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	vkCmdPipelineBarrier(
		cmdBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, 
		VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT 
		| VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_DEPENDENCY_DEVICE_GROUP_BIT,
		0, nullptr,
		1, &barrier,
		0, nullptr);
	
	theirAllocationSubmitter.QueueAllocSubmission(std::move(allocSubID));
}


std::tuple<VkResult, VkBuffer, VkDeviceMemory>
BufferAllocator::CreateBuffer(
	AllocationSubmissionID					allocSubID,
	VkBufferUsageFlags						usage,
	const void*								data,
	size_t									size,
	const std::vector<QueueFamilyIndex>&	owners,
	VkMemoryPropertyFlags					memPropFlags)
{
	VkBuffer buffer{};
	VkDeviceMemory memory{};
	
	// BUFFER CREATION
	VkBufferCreateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.flags = NULL;
	bufferInfo.usage = usage;

	if (IsExclusive(owners.data(), owners.size()))
	{
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.pQueueFamilyIndices = nullptr;
		bufferInfo.queueFamilyIndexCount = 0;
	}
	else
	{
		bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferInfo.pQueueFamilyIndices = owners.data();
		bufferInfo.queueFamilyIndexCount = owners.size();
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

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	VkMemoryAllocateFlagsInfo allocFlagsInfo = {};
	allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
	allocFlagsInfo.flags = usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : NULL;
	allocInfo.pNext = &allocFlagsInfo;

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

			auto [resultStaged, stagedBuffer] = CreateStagingBuffer(data, size, owners.data(), owners.size());

			auto& allocSub = theirAllocationSubmitter[allocSubID];
			vkCmdCopyBuffer(allocSub.Record() , stagedBuffer.buffer, buffer, 1, &copy);
			allocSub.AddResourceBuffer(stagedBuffer.buffer, stagedBuffer.memory);
		}
		else
		{
			LOG("WARNING: data was present but neither transfer source or transfer destination was included in buffer usage");
		}
	}
	return {VK_SUCCESS, buffer, memory};
}

void
BufferAllocator::QueueDestroy(
	VkBuffer&&														buffer,
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	waitSignal)
{
	QueuedBufferDestroy queuedDestroy;
	queuedDestroy.buffer		= buffer;
	queuedDestroy.waitSignal	= std::move(waitSignal);
	myBufferDestroyQueue.push(queuedDestroy);
}

void
BufferAllocator::DoCleanUp(
	int limit)
{
	{
		AllocatedBuffer allocBuffer;
		int count = 0;
		while (myRequestedBuffersQueue.try_pop(allocBuffer)
			&& count++ < limit)
		{
			myAllocatedBuffers[allocBuffer.buffer] = allocBuffer;
		}
	}
	{
		QueuedBufferDestroy queuedDestroy;
		int count = 0;

		while (myBufferDestroyQueue.try_pop(queuedDestroy)
			&& count++ < limit)
		{
			if (!myAllocatedBuffers.contains(queuedDestroy.buffer))
			{
				// Requested resource may not be popped from queue yet
				myFailedDestructs.emplace_back(queuedDestroy);
				continue;
			}
			if (!SemaphoreWait(*queuedDestroy.waitSignal))
			{
				myFailedDestructs.emplace_back(queuedDestroy);
				continue;
			}
			const auto& allocBuffer = myAllocatedBuffers[queuedDestroy.buffer];
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocBuffer.buffer, nullptr);
			vkFreeMemory(theirVulkanFramework.GetDevice(), allocBuffer.memory, nullptr);
			myAllocatedBuffers.erase(queuedDestroy.buffer);
		}
		for (auto&& failedDestroy : myFailedDestructs)
		{
			failedDestroy.tries++;
			myBufferDestroyQueue.push(failedDestroy);
		}
		myFailedDestructs.clear();
	}
}







