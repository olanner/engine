#include "pch.h"
#include "BufferAllocator.h"

#include "ImmediateTransferrer.h"
#include "Reflex/VK/Geometry/Vertex2D.h"
#include "Reflex/VK/Geometry/Vertex3D.h"
#include "Reflex/VK/VulkanFramework.h"

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
	AllocationSubmission& allocSub,
	const std::vector<Vertex3D>& vertices, 
	const std::vector<QueueFamilyIndex>& owners)
{
	auto [result, buffer, memory] = CreateBuffer(
																		allocSub,
																		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
																		vertices.data(),
																		sizeof(Vertex3D) * vertices.size(),
																		owners,
																		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
																		false);
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
	AllocationSubmission&					allocSub,
	const std::vector<struct Vertex2D>&		vertices,
	const std::vector<QueueFamilyIndex>&	owners)
{
	auto [result, buffer, memory] = CreateBuffer(
													allocSub,
													VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
													vertices.data(),
													sizeof(Vertex2D) * vertices.size(),
													owners,
													VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
													false
	);
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
	AllocationSubmission&					allocSub,
	const std::vector<uint32_t>&			indices,
	const std::vector<QueueFamilyIndex>&	owners)
{
	auto [result, buffer, memory] = CreateBuffer(
										allocSub,
										VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
										indices.data(),
										sizeof(uint32_t) * indices.size(),
										owners,
										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
										false
	);
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
	AllocationSubmission&					allocSub,
	const void*								startData,
	size_t									size,
	const std::vector<QueueFamilyIndex>&	owners,
	VkMemoryPropertyFlags					memPropFlags)
{
	auto [result, buffer, memory] = CreateBuffer(
									allocSub,
									VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
									startData,
									size,
									owners,
									memPropFlags,
									false
	);
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
	VkBuffer	buffer,	
	const void* data)
{
	// TODO: maybe do this better lol
	auto allocSub = theirAllocationSubmitter.StartAllocSubmission();
	const auto cmdBuffer = allocSub.Record();
	
	vkCmdUpdateBuffer(cmdBuffer, buffer, 0, myAllocatedBuffers[buffer].size, data);
	
	VkBufferMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	
	barrier.buffer = buffer;
	barrier.size = myAllocatedBuffers[buffer].size;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	
	vkCmdPipelineBarrier(cmdBuffer,
						  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
						  VK_DEPENDENCY_DEVICE_GROUP_BIT,
						  0, nullptr,
						  1, &barrier,
						  0, nullptr);
	
	theirAllocationSubmitter.QueueAllocSubmission(std::move(allocSub));
}


std::tuple<VkResult, VkBuffer, VkDeviceMemory>
BufferAllocator::CreateBuffer(
	AllocationSubmission&					allocSub,
	VkBufferUsageFlags						usage,
	const void*								data,
	size_t									size,
	const std::vector<QueueFamilyIndex>&	owners,
	VkMemoryPropertyFlags					memPropFlags,
	bool									immediateTransfer)
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
				vkCmdCopyBuffer(allocSub.Record() , stagedBuffer.buffer, buffer, 1, &copy);
				
				allocSub.AddStagingBuffer(std::move(stagedBuffer));
			}
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







