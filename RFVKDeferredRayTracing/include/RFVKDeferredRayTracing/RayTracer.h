#pragma once
#include "Shared.h"
#include "RFVK/Ray Tracing/NVRayTracing.h"
#include "RFVK/Mesh/MeshRendererBase.h"
#include "RFVK/Pipelines/Pipeline.h"

class RayTracer final 
{
public:
	RayTracer(
		class VulkanFramework&				vulkanFramework,
		class MeshHandler&					meshHandler,
		class ImageHandler&					imageHandler,
		class SceneGlobals&					sceneGlobals,
		class BufferAllocator&				bufferAllocator,
		class AccelerationStructureHandler& accStructHandler,
		const GBuffer&						gBuffer,
		QueueFamilyIndices					familyIndices);
	~RayTracer();

	void	Record(
				int				swapchainIndex,
				VkCommandBuffer	cmdBuffer,
				const MeshRenderSchedule::AssembledWorkType& 
								assembledWork);

private:
	ShaderBindingTable					CreateShaderBindingTable(
		AllocationSubmissionID	allocSubID,
		size_t					handleSize,
		uint32_t				firstGroup,
		uint32_t				groupCount,
		VkPipeline				pipeline);
	uint32_t							AlignedSize(
		uint32_t a,
		uint32_t b);
	
	VulkanFramework&					theirVulkanFramework;
	MeshHandler&						theirMeshHandler;
	ImageHandler&						theirImageHandler;
	SceneGlobals&						theirSceneGlobals;
	BufferAllocator&					theirBufferAllocator;
	AccelerationStructureHandler&		theirAccStructHandler;

	GBuffer								myGBuffer = {};
	Pipeline							myPipeline = {};
	std::shared_ptr<class Shader>		myOpaqueShader;

	std::array<ShaderBindingTable, 3>	myShaderBindingTables;

	InstanceStructID					myInstancesID = InstanceStructID(-1);
	RTInstances							myInstances;

};