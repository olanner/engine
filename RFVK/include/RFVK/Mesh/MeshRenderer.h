
#pragma once
#include "MeshRendererBase.h"
#include "RFVK/Pipelines/Pipeline.h"
#include "RFVK/RenderPass/RenderPassFactory.h"
#include "RFVK/WorkerSystem/WorkerSystem.h"

struct Instance
{
	Mat4f		mat;
	Vec3f		padding0;
	uint32_t	objID;
};
static_assert(128 > sizeof Instance);
struct UniformInstances
{
	Instance instances[MaxNumInstances];
};

class MeshRenderer final : public MeshRendererBase
{
public:
										MeshRenderer(
											class VulkanFramework&		vulkanFramework,
											class UniformHandler&		uniformHandler,
											class MeshHandler&			meshHandler,
											class ImageHandler&			imageHandler,
											class SceneGlobals&			sceneGlobals,
											class RenderPassFactory&	renderPassFactory,
											QueueFamilyIndices			familyIndices);
										~MeshRenderer();

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
										RecordSubmit(
											uint32_t swapchainImageIndex, 
											const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores, 
											const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores) override;
	std::vector<rflx::Features>			GetImplementedFeatures() const override;
	int									GetSubmissionCount() override { return 1; }

private:
	RenderPassFactory&					theirRenderPassFactory;
	const VkPipelineStageFlags			myWaitStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	
	std::unique_ptr<UniformInstances>	myInstanceData;
	UniformID							myInstanceUniformID = UniformID(INVALID_ID);

	RenderPass							myDeferredRenderPass;

	class Shader*						myDeferredGeoShader;
	Pipeline							myDeferredGeoPipeline;

	class Shader*						myDeferredLightShader;
	Pipeline							myDeferredLightPipeline;

};