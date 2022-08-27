
#pragma once
#include "NVRayTracing.h"
#include "RFVK/Misc/HandlerBase.h"



struct InstanceStructure
{
	std::array<VkAccelerationStructureKHR, NumSwapchainImages>
													structures;
	std::array<VkWriteDescriptorSetAccelerationStructureKHR, NumSwapchainImages>
													infos;
};

struct GeometryStructure
{
	VkAccelerationStructureKHR						structure;
	VkDeviceAddress									address;
};

class AccelerationStructureHandler : public HandlerBase
{
public:
									AccelerationStructureHandler(
										class VulkanFramework&					vulkanFramework,
										class AccelerationStructureAllocator&	accStructAllocator);
									~AccelerationStructureHandler();

	GeoStructID						AddGeometryStructure();
	VkResult						LoadGeometryStructure(
										GeoStructID					geoStructID,
										AllocationSubmissionID		allocSubID,
										const struct MeshGeometry&	mesh);
	void							UnloadGeometryStructure(GeoStructID geoID);
	void							SignalUnload(
										int		swapchainIndex,
										VkFence fence);

	InstanceStructID				AddInstanceStructure(
										AllocationSubmissionID		allocSubID,
										const RTInstances&			instances);
	void							UpdateInstanceStructure(
										int					swapchainIndex,
										InstanceStructID	id, 
										const RTInstances&	instances);

	VkDescriptorSetLayout			GetInstanceStructuresLayout() const;

	GeometryStructure				operator[](GeoStructID geoStructID);

	void							BindInstanceStructures(
										int					swapchainIndex,
										VkCommandBuffer		commandBuffer,
										VkPipelineLayout	pipelineLayout,
										uint32_t			setIndex) const;

	


private:
	AccelerationStructureAllocator& theirAccStructAllocator;

	VkDescriptorPool				myDescriptorPool = nullptr;
	std::array<conc_queue<shared_semaphore<NumSwapchainImages>>, NumSwapchainImages>
									myQueuedUnloadSignals;

	GeometryStructure				myDefaultGeoStructure = {};
	std::array<GeometryStructure, MaxNumMeshesLoaded>
									myGeometryStructures = {};
	IDKeeper<GeoStructID>			myFreeGeoIDs;


	VkDescriptorSetLayout			myInstanceStructDescriptorSetLayout = nullptr;
	std::array<VkDescriptorSet, NumSwapchainImages>
									myInstanceStructDescriptorSets = {};

	std::array<InstanceStructure, MaxNumInstanceStructures>
									myInstanceStructures = {};
	IDKeeper<InstanceStructID>		myFreeInstanceIDs;

};
