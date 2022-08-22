
#pragma once
#include "NVRayTracing.h"
#include "Reflex/VK/Misc/HandlerBase.h"

struct InstanceStructure
{
	VkWriteDescriptorSetAccelerationStructureNV info;
	VkAccelerationStructureNV					structure;
};

class AccelerationStructureHandler : public HandlerBase
{
public:
									AccelerationStructureHandler(
										class VulkanFramework&					vulkanFramework,
										class AccelerationStructureAllocator&	accStructAllocator);
									~AccelerationStructureHandler();

	VkResult						LoadGeometryStructure(
										MeshID						meshID,
										class AllocationSubmission& allocSub,
										const struct MeshGeometry&	mesh);
	void							UnloadGeometryStructure(MeshID meshID);
	void							SignalUnload(
										int		swapchainIndex,
										VkFence fence);

	InstanceStructID				AddInstanceStructure(
										class AllocationSubmission& allocSub,
										const RTInstances&			instances);
	void							UpdateInstanceStructure(
										InstanceStructID	id, 
										const RTInstances&	instances);

	VkDescriptorSetLayout			GetInstanceStructuresLayout() const;
	VkDescriptorSet					GetInstanceStructuresSet() const;

	VkAccelerationStructureNV		GetGeometryStruct(MeshID id);

	void							BindInstanceStructures(
										VkCommandBuffer		commandBuffer,
										VkPipelineLayout	pipelineLayout,
										uint32_t			setIndex) const;

	


private:
	AccelerationStructureAllocator& theirAccStructAllocator;

	VkAccelerationStructureNV		myDefaultGeoStructure;
	std::array<VkAccelerationStructureNV, MaxNumMeshesLoaded>
									myGeometryStructures;
	std::array<conc_queue<shared_semaphore<NumSwapchainImages>>, NumSwapchainImages>
									myQueuedUnloadSignals;

	VkDescriptorPool				myDescriptorPool;

	VkDescriptorSetLayout			myInstanceStructDescriptorSetLayout;
	VkDescriptorSet					myInstanceStructDescriptorSet;
	std::array<InstanceStructure, MaxNumInstanceStructures>
									myInstanceStructures;
	concurrency::concurrent_priority_queue<InstanceStructID, std::greater<>>
									myFreeIDs;

};
