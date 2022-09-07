
#pragma once
#include "NVRayTracing.h"
#include "RFVK/Mesh/MeshRendererBase.h"
#include "RFVK/Pipelines/Pipeline.h"

class RTMeshRenderer final : public MeshRendererBase
{
public:
										RTMeshRenderer(
											class VulkanFramework&				vulkanFramework,
											class UniformHandler&				uniformHandler,
											class MeshHandler&					meshHandler,
											class ImageHandler&					imageHandler,
											class SceneGlobals&					sceneGlobals,
											class BufferAllocator&				bufferAllocator,
											class AccelerationStructureHandler& accStructHandler,
											QueueFamilyIndices					familyIndices);
										~RTMeshRenderer();

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
										RecordSubmit(
											uint32_t	swapchainImageIndex, 
											const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&	
														waitSemaphores, 
											const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&	
														signalSemaphores) override;
	std::vector<rflx::Features>			GetImplementedFeatures() const override;
	int									GetSubmissionCount() override { return 1; }

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
	
	BufferAllocator&					theirBufferAllocator;
	AccelerationStructureHandler&		theirAccStructHandler;

	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
										myWaitStages;
	Pipeline							myPipeline;
	std::shared_ptr<class Shader>		myOpaqueShader;

	std::array<ShaderBindingTable, 3>	myShaderBindingTables;

	InstanceStructID					myInstancesID;
	RTInstances							myInstances;

};
