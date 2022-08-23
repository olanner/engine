
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
														QueueFamilyIndex			cmdBufferFamily,
														QueueFamilyIndex			transferFamily);
													~Presenter();

	[[nodiscard]] std::tuple<VkSubmitInfo, VkFence> RecordSubmit(
														uint32_t				swapchainImageIndex,
														VkSemaphore*			waitSemaphores,
														uint32_t				numWaitSemaphores,
														VkPipelineStageFlags*	waitPipelineStages,
														uint32_t				numWaitStages,
														VkSemaphore*			signalSemaphore) override;
	std::array<VkFence, NumSwapchainImages>			GetFences() override;
	std::vector<rflx::Features>						GetImplementedFeatures() const override;

private:
	VulkanFramework&								theirVulkanFramework;
	RenderPassFactory&								theirRenderPassFactory;
	ImageHandler&									theirImageHandler;
	SceneGlobals&									theirSceneGlobals;

	class Shader*									myPresentShader;
	Pipeline										myPresentPipeline;
	RenderPass										myPresentRenderPass;

	std::array<VkCommandBuffer, NumSwapchainImages> myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;

};