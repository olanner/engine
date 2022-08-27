
#pragma once
#include "NVRayTracing.h"
#include "RFVK/Memory/AllocatorBase.h"

struct AllocatedAccelerationStructure
{
	VkAccelerationStructureKHR	structure			= nullptr;
	BufferXMemory				structureBuffer		= {};
	BufferXMemory				scratchBuffer		= {};
	BufferXMemory				instancesBuffer		= {};
	VkDeviceAddress				structureAddress	= 0;
	VkDeviceAddress				scratchAddress		= 0;
	VkDeviceAddress				instancesAddress	= 0;
	size_t						instancesBufferSize	= 0;
};

struct QueuedAccStructDestroy
{
	VkAccelerationStructureKHR										structure = nullptr;
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

	std::tuple<VkResult, VkAccelerationStructureKHR> RequestGeometryStructure(
														AllocationSubmissionID					allocSubID,
														const std::vector<struct MeshGeometry>&	meshes);
	std::tuple<VkResult, VkAccelerationStructureKHR> RequestInstanceStructure(
														AllocationSubmissionID	allocSubID,	
														const RTInstances&		instanceDesc);

	void											UpdateInstanceStructure(
														VkAccelerationStructureKHR	instanceStructure, 
														const RTInstances&			instanceDesc);

	void											QueueDestroy(
														VkAccelerationStructureKHR	accStruct, 
														std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	
																					waitSignal);
	void											DoCleanUp(int limit) override;
	BufferAllocator&								GetBufferAllocator() const;
	std::vector<QueueFamilyIndex>					GetOwners() const;

private:
	void											FreeAllocatedStructure(
														const AllocatedAccelerationStructure& allocatedStructure) const;

	void											BuildInstanceStructure(
														AllocationSubmissionID		allocSubID,
														bool						update,
														const RTInstances&			instanceDesc,
														VkBuffer					instancesBuffer,
														size_t						instancesBufferSize,
														VkDeviceAddress				instancesBufferAddress,
														VkDeviceAddress				scratchBufferAddress,
														VkAccelerationStructureKHR	instanceStructure);
	

	BufferAllocator&								theirBufferAllocator;

	std::vector<QueueFamilyIndex>					myOwners;
	QueueFamilyIndex								myTransferFamilyIndex,
													myPresentationFamilyIndex;

	conc_queue<AllocatedAccelerationStructure>		myQueuedRequests;
	std::unordered_map<VkAccelerationStructureKHR, AllocatedAccelerationStructure>
													myAllocatedAccelerationStructures;
	conc_queue<QueuedAccStructDestroy>				myQueuedDestroys;
	std::vector<QueuedAccStructDestroy>				myFailedDestroys;

};
