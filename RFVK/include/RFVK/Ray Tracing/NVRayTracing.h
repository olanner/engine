
#pragma once

//inline std::function<decltype(vkGetAccelerationStructureMemoryRequirementsNV)> vkGetAccelerationStructureMemoryRequirements;
//inline std::function<decltype(vkCreateAccelerationStructureNV)> vkCreateAccelerationStructure;
//inline std::function<decltype(vkBindAccelerationStructureMemoryNV)> vkBindAccelerationStructureMemory;
//inline std::function<decltype(vkDestroyAccelerationStructureNV)> vkDestroyAccelerationStructure;
//
//inline std::function<decltype(vkGetAccelerationStructureHandleNV)> vkGetAccelerationStructureHandle;
//
//inline std::function<decltype(vkCmdBuildAccelerationStructureNV)> vkCmdBuildAccelerationStructure;
//inline std::function<decltype(vkCmdCopyAccelerationStructureNV)> vkCmdCopyAccelerationStructure;
//inline std::function<decltype(vkCmdWriteAccelerationStructuresPropertiesNV)> vkCmdWriteAccelerationStructuresProperties;
//
//inline std::function<decltype(vkGetRayTracingShaderGroupHandlesNV)> vkGetRayTracingShaderGroupHandles;
//inline std::function<decltype(vkCreateRayTracingPipelinesNV)> vkCreateRayTracingPipelines;
//inline std::function<decltype(vkCmdTraceRaysNV)> vkCmdTraceRays;
//
//inline std::function<decltype(vkCompileDeferredNV)> vkCompileDeferred;
inline std::function<decltype(vkCreateAccelerationStructureKHR)>			vkCreateAccelerationStructure;
inline std::function<decltype(vkDestroyAccelerationStructureKHR)>			vkDestroyAccelerationStructure;
inline std::function<decltype(vkCmdBuildAccelerationStructuresKHR)>			vkCmdBuildAccelerationStructures;
inline std::function<decltype(vkGetAccelerationStructureDeviceAddressKHR)>	vkGetAccelerationStructureDeviceAddress;
inline std::function<decltype(vkGetAccelerationStructureBuildSizesKHR)>		vkGetAccelerationStructureBuildSizes;
inline std::function<decltype(vkGetRayTracingShaderGroupHandlesKHR)>		vkGetRayTracingShaderGroupHandles;
inline std::function<decltype(vkCmdTraceRaysKHR)>							vkCmdTraceRays;
inline std::function<decltype(vkCreateRayTracingPipelinesKHR)>				vkCreateRayTracingPipelines;

using RTInstances = neat::static_vector<VkAccelerationStructureInstanceKHR, MaxNumInstances>;

struct ShaderBindingTable
{
	VkBuffer						buffer;
	VkDeviceMemory					memory;
	VkStridedDeviceAddressRegionKHR stride;
};