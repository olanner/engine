
#pragma once
#include "NVRayTracing.h"

class AccelerationStructureHandler
{
public:
									AccelerationStructureHandler(
										class VulkanFramework&					vulkanFramework,
										class AccelerationStructureAllocator&	accStructAllocator);
									~AccelerationStructureHandler();

	VkResult						AddGeometryStructure(
										class AllocationSubmission& allocSub,
										MeshID						ownerID,
										const struct Mesh*			firstMesh, 
										uint32_t					numMeshes);

	InstanceStructID				AddInstanceStructure(
										class AllocationSubmission& allocSub,
										const RTInstances&			instances);
	void							UpdateInstanceStructure(
										InstanceStructID	id, 
										const RTInstances&	instances);

	VkDescriptorSetLayout			GetInstanceStructuresLayout();
	VkDescriptorSet					GetInstanceStructuresSet();

	VkAccelerationStructureNV		GetGeometryStruct(MeshID id);

	void							BindInstanceStructures(
										VkCommandBuffer		commandBuffer,
										VkPipelineLayout	pipelineLayout,
										uint32_t			setIndex);


private:
	VulkanFramework&				theirVulkanFramework;
	AccelerationStructureAllocator& theirAccStructAllocator;

	std::array<VkAccelerationStructureNV, MaxNumMeshesLoaded>
									myGeometryStructures;

	VkDescriptorPool				myDescriptorPool;

	VkDescriptorSetLayout			myInstanceStructDescriptorSetLayout;
	VkDescriptorSet					myInstanceStructDescriptorSet;
	std::array<VkAccelerationStructureNV, MaxNumInstanceStructures>
									myInstanceStructures;
	concurrency::concurrent_priority_queue<InstanceStructID, std::greater<>>
									myFreeIDs;

};
