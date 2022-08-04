
#pragma once
#include "NVRayTracing.h"
#include "Reflex/VK/Memory/AllocatorBase.h"


class AccelerationStructureAllocator : public AllocatorBase
{
public:
													AccelerationStructureAllocator(
														class VulkanFramework&		vulkanFramework,
														AllocationSubmitter&		allocationSubmitter,
														class BufferAllocator&		bufferAllocator,
														class ImmediateTransferrer& immediateTransferrer,
														QueueFamilyIndex			transferFamilyIndex,
														QueueFamilyIndex			presentationFamilyIndex);

													~AccelerationStructureAllocator();

	std::tuple<VkResult, VkAccelerationStructureNV> RequestGeometryStructure(
														AllocationSubmission&	allocSub,
														const struct Mesh*		firstMesh,
														uint32_t				numMeshes);
	std::tuple<VkResult, VkAccelerationStructureNV> RequestInstanceStructure(
														AllocationSubmission&	allocSub,	
														const RTInstances&		instanceDesc);

	VkBuffer										GetUnderlyingBuffer(
														VkAccelerationStructureNV accStruct);

	void											UpdateInstanceStructure(
														VkAccelerationStructureNV	instanceStructure, 
														const RTInstances&			instanceDesc);

private:
	std::tuple<VkMemoryRequirements2, MemTypeIndex> GetMemReq(
														VkAccelerationStructureNV						accStruct,
														VkMemoryPropertyFlags							memPropFlags,
														VkAccelerationStructureMemoryRequirementsTypeNV accMemType);

	ImmediateTransferrer&							theirImmediateTransferrer;
	BufferAllocator&								theirBufferAllocator;

	std::vector<QueueFamilyIndex>					myOwners;
	QueueFamilyIndex								myTransferFamilyIndex,
													myPresentationFamilyIndex;

	std::unordered_map<VkAccelerationStructureNV, VkDeviceMemory>
													myAccelerationStructsMemory;
	std::unordered_map<VkAccelerationStructureNV, VkBuffer>
													myAccelerationStructsBuffers;

	size_t											myInstanceDescSize;
	VkBuffer										myInstanceDescBuffer;
	VkDeviceMemory									myInstanceDescMemory;

	size_t											myInstanceScratchSize;
	VkBuffer										myInstanceScratchBuffer;
	VkDeviceMemory									myInstanceScratchMemory;

	size_t											myInstanceObjMaxSize;

};
