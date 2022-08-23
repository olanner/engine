
#pragma once

inline std::function<decltype(vkGetAccelerationStructureMemoryRequirementsNV)> vkGetAccelerationStructureMemoryRequirements;
inline std::function<decltype(vkCreateAccelerationStructureNV)> vkCreateAccelerationStructure;
inline std::function<decltype(vkBindAccelerationStructureMemoryNV)> vkBindAccelerationStructureMemory;
inline std::function<decltype(vkDestroyAccelerationStructureNV)> vkDestroyAccelerationStructure;

inline std::function<decltype(vkGetAccelerationStructureHandleNV)> vkGetAccelerationStructureHandle;

inline std::function<decltype(vkCmdBuildAccelerationStructureNV)> vkCmdBuildAccelerationStructure;
inline std::function<decltype(vkCmdCopyAccelerationStructureNV)> vkCmdCopyAccelerationStructure;
inline std::function<decltype(vkCmdWriteAccelerationStructuresPropertiesNV)> vkCmdWriteAccelerationStructuresProperties;

inline std::function<decltype(vkGetRayTracingShaderGroupHandlesNV)> vkGetRayTracingShaderGroupHandles;
inline std::function<decltype(vkCreateRayTracingPipelinesNV)> vkCreateRayTracingPipelines;
inline std::function<decltype(vkCmdTraceRaysNV)> vkCmdTraceRays;

inline std::function<decltype(vkCompileDeferredNV)> vkCompileDeferred;

struct GeometryInstance
{
	Mat4nv transform;
	uint32_t customID : 24;
	uint32_t mask : 8;
	uint32_t instanceOffset : 24;
	uint32_t flags : 8;
	uint64_t accelerationStructureHandle;
};

static_assert(sizeof GeometryInstance == 64);

using RTInstances = neat::static_vector<GeometryInstance, MaxNumMeshesLoaded>;