
#pragma once

#include "AllocatorBase.h"

struct AllocatedBuffer
{
	VkBuffer		buffer	= nullptr;
	VkDeviceMemory	memory	= nullptr;
	size_t			size	= 0;
};

struct QueuedBufferDestroy
{
	VkBuffer														buffer = nullptr;
	int																tries = 0;
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	waitSignal;
};

class BufferAllocator : public AllocatorBase
{
	friend class AccelerationStructureAllocator;
	
public:
	using AllocatorBase::AllocatorBase;
	~BufferAllocator();

	std::tuple<VkResult, VkBuffer>					RequestVertexBuffer(
														AllocationSubmissionID					allocSubID,
														const std::vector<struct Vertex3D>&		vertices, 
														const std::vector<QueueFamilyIndex>&	owners);
	std::tuple<VkResult, VkBuffer>					RequestVertexBuffer(
														AllocationSubmissionID					allocSubID,
														const std::vector<struct Vertex2D>&		vertices,
														const std::vector<QueueFamilyIndex>&	owners);
	
	std::tuple<VkResult, VkBuffer>					RequestIndexBuffer(
														AllocationSubmissionID					allocSubID,
														const std::vector<uint32_t>&			indices,
														const std::vector<QueueFamilyIndex>&	owners);
	std::tuple<VkResult, VkBuffer>					RequestUniformBuffer(
														AllocationSubmissionID					allocSubID,
														const void*								startData,
														size_t									size,
														const std::vector<QueueFamilyIndex>&	owners,
														VkMemoryPropertyFlags					memPropFlags);

	void											RequestBufferView(VkBuffer	buffer);

	void											UpdateBufferData(
														VkBuffer								buffer, 
														const void*								data,
														size_t									offset,
														size_t									size,
														const std::vector<QueueFamilyIndex>&	owners);
	[[nodiscard]] std::tuple<VkResult, VkBuffer, VkDeviceMemory>
													CreateBuffer(
														AllocationSubmissionID					allocSubID,
														VkBufferUsageFlags						usage,
														const void*								data,
														size_t									size,
														const std::vector<QueueFamilyIndex>&	owners,
														VkMemoryPropertyFlags					memPropFlags);

	void											QueueDestroy(
														VkBuffer&&								buffer,
														std::shared_ptr<std::counting_semaphore<NumSwapchainImages>> 
																								waitSignal);
	void											DoCleanUp(int limit) override;

private:
	//std::unordered_map<VkBuffer, VkDeviceMemory>	myAllocatedBuffers;
	//std::unordered_map<VkBuffer, size_t>			myBufferSizes;
	//std::unordered_map<VkBuffer, VkBufferView>		myBufferViews;
	std::unordered_map<VkBuffer, AllocatedBuffer>	myAllocatedBuffers;
	conc_queue<AllocatedBuffer>						myRequestedBuffersQueue;
	conc_queue<QueuedBufferDestroy>					myBufferDestroyQueue;
	std::vector<QueuedBufferDestroy>				myFailedDestructs;


};

