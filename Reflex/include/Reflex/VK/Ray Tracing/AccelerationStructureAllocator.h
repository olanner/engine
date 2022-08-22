
#pragma once
#include "NVRayTracing.h"
#include "Reflex/VK/Memory/AllocatorBase.h"

struct AllocatedAccelerationStructure
{
	VkAccelerationStructureNV	structure;
	VkBuffer					buffer;
	VkDeviceMemory				memory;
};

struct QueuedAccStructDestroy
{
	VkAccelerationStructureNV										structure;
	int																tries = 0;
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	waitSignal;
};

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
														AllocationSubmission&					allocSub,
														const std::vector<struct MeshGeometry>&	meshes);
	std::tuple<VkResult, VkAccelerationStructureNV> RequestInstanceStructure(
														AllocationSubmission&	allocSub,	
														const RTInstances&		instanceDesc);

	VkBuffer										GetUnderlyingBuffer(
														VkAccelerationStructureNV accStruct);

	void											UpdateInstanceStructure(
														VkAccelerationStructureNV	instanceStructure, 
														const RTInstances&			instanceDesc);

	void											QueueDestroy(
														VkAccelerationStructureNV	accStruct, 
														std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	
																					waitSignal);
	void											DoCleanUp(int limit) override;
	BufferAllocator&								GetBufferAllocator() const;
	std::vector<QueueFamilyIndex>					GetOwners() const;

private:
	std::tuple<VkMemoryRequirements2, MemTypeIndex> GetMemReq(
														VkAccelerationStructureNV						accStruct,
														VkMemoryPropertyFlags							memPropFlags,
														VkAccelerationStructureMemoryRequirementsTypeNV accMemType);

	BufferAllocator&								theirBufferAllocator;

	std::vector<QueueFamilyIndex>					myOwners;
	QueueFamilyIndex								myTransferFamilyIndex,
													myPresentationFamilyIndex;

	conc_queue<AllocatedAccelerationStructure>		myQueuedRequests;
	std::unordered_map<VkAccelerationStructureNV, AllocatedAccelerationStructure>
													myAllocatedAccelerationStructures;
	conc_queue<QueuedAccStructDestroy>				myQueuedDestroys;
	std::vector<QueuedAccStructDestroy>				myFailedDestroys;

	size_t											myInstanceDescSize;
	VkBuffer										myInstanceDescBuffer;
	VkDeviceMemory									myInstanceDescMemory;

	size_t											myInstanceScratchSize;
	VkBuffer										myInstanceScratchBuffer;
	VkDeviceMemory									myInstanceScratchMemory;

	size_t											myInstanceObjMaxSize;

};
