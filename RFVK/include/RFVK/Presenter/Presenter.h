
#pragma once
#include "RFVK/Pipelines/Pipeline.h"
#include "RFVK/RenderPass/RenderPass.h"
#include "RFVK/WorkerSystem/WorkerSystem.h"

class Presenter : public WorkerSystem
{
public:
													Presenter(
														class VulkanFramework&		vulkanFramework,
														class RenderPassFactory&	renderPassFactory,
														class ImageHandler&			imageHandler,
														class SceneGlobals&			sceneGlobals,
														QueueFamilyIndices			familyIndices);
													~Presenter();

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
													RecordSubmit(
														uint32_t swapchainImageIndex, 
														const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores, 
														const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores) override;
	std::array<VkFence, NumSwapchainImages>			GetFences() override;
	std::vector<rflx::Features>						GetImplementedFeatures() const override;
	int												GetSubmissionCount() override { return 1; }

private:
	VulkanFramework&								theirVulkanFramework;
	RenderPassFactory&								theirRenderPassFactory;
	ImageHandler&									theirImageHandler;
	SceneGlobals&									theirSceneGlobals;

	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
													myWaitStages;
	class Shader*									myPresentShader;
	Pipeline										myPresentPipeline;
	RenderPass										myPresentRenderPass;

	std::array<VkCommandBuffer, NumSwapchainImages> myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;

};