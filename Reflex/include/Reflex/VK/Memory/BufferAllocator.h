
#pragma once

#include "AllocatorBase.h"


struct BufferAllotment
{
	char key[32];
	uint32_t size;
	VkDeviceMemory memory;
};

class BufferAllocator : public AllocatorBase
{
	friend class AccelerationStructureAllocator;
	
public:
	using AllocatorBase::AllocatorBase;
	~BufferAllocator();

	std::tuple<VkResult, VkBuffer>					RequestVertexBuffer(
														const struct Vertex3D*	firstVertex,
														uint32_t				numVertices,
														const QueueFamilyIndex* firstOwner,
														uint32_t				numOwners);
	std::tuple<VkResult, VkBuffer>					RequestVertexBuffer(
														const struct Vertex2D*	firstVertex,
														uint32_t				numVertices,
														const QueueFamilyIndex* firstOwner,
														uint32_t				numOwners);
	
	std::tuple<VkResult, VkBuffer>					RequestIndexBuffer(
														const uint32_t*			firstIndex,
														uint32_t				numIndices,
														const QueueFamilyIndex* firstOwner,
														uint32_t				numOwners);
	std::tuple<VkResult, VkBuffer>					RequestUniformBuffer(
														const void*				startData,
														size_t					size,
														const QueueFamilyIndex* firstOwner,
														uint32_t				numOwners,
														VkMemoryPropertyFlags	memPropFlags);

	void											RequestBufferView(VkBuffer	buffer);

	void											UpdateBufferData(
														VkBuffer	buffer, 
														const void* data);
	[[nodiscard]] std::tuple<VkResult, VkBuffer, VkDeviceMemory>
													CreateBuffer(
														VkBufferUsageFlags		usage,
														const void*				data,
														size_t					size,
														const QueueFamilyIndex* firstOwner,
														uint32_t				numOwners,
														VkMemoryPropertyFlags	memPropFlags,
														bool					immediateTransfer = false);

private:
	std::unordered_map<VkBuffer, VkDeviceMemory>	myAllocatedBuffers;
	std::unordered_map<VkBuffer, size_t>			myBufferSizes;
	std::unordered_map<VkBuffer, VkBufferView>		myBufferViews;



};

