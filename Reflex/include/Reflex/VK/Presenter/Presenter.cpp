#include "pch.h"
#include "Presenter.h"


#include "Reflex/VK/Image/ImageHandler.h"
#include "Reflex/VK/Memory/BufferAllocator.h"
#include "Reflex/VK/Pipelines/FullscreenPipeline.h"
#include "Reflex/VK/Shader/Shader.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Pipelines/PipelineBuilder.h"
#include "Reflex/VK/RenderPass/RenderPassFactory.h"
#include "Reflex/VK/Scene/SceneGlobals.h"

Presenter::Presenter(
	VulkanFramework&	vulkanFramework,
	RenderPassFactory&	renderPassFactory,
	ImageHandler&		imageHandler,
	SceneGlobals&		sceneGlobals,
	QueueFamilyIndex	cmdBufferFamily,
	QueueFamilyIndex	transferFamily)
	: theirVulkanFramework(vulkanFramework)
	, theirRenderPassFactory(renderPassFactory)
	, theirImageHandler(imageHandler)
	, theirSceneGlobals(sceneGlobals)
{
	QueueFamilyIndex qIndices[2]{cmdBufferFamily, transferFamily};

	VkResult result{};

	// RENDER PASS
	auto [w, h] = theirVulkanFramework.GetTargetResolution();

	auto rpConstructor = theirRenderPassFactory.GetConstructor();
	rpConstructor.DefineAttachments({SwapchainAttachment, IntermediateAttachment}, {w,h});
	rpConstructor.AddSubpass()
		.SetColorAttachments({0})
		.SetInputAttachments({1});
	rpConstructor.AddSubpass()
		.SetColorAttachments({1});

	rpConstructor.AddSubpassDependency(
	{
		VK_SUBPASS_EXTERNAL,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		NULL,
		VK_ACCESS_MEMORY_WRITE_BIT,
		VK_DEPENDENCY_BY_REGION_BIT
	});

	rpConstructor.AddSubpassDependency(
	{
		0,
		1,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		VK_DEPENDENCY_BY_REGION_BIT
	});

	myPresentRenderPass = rpConstructor.Build();

	// SHADER
	char paths[][128]
	{
		"Shaders/fullscreen_vshader.vert",
		"Shaders/present_fshader.frag",
	};
	myPresentShader = new Shader(paths, 2, theirVulkanFramework);

	// PIPELINE
	PipelineBuilder pipelineConstructor(1, myPresentRenderPass.renderPass, myPresentShader);
	pipelineConstructor.DefineVertexInput(nullptr);
	pipelineConstructor.DefineViewport({w,h}, {0,0,w, h});
	pipelineConstructor.SetDepthEnabled(false);

	std::tie(result, myPresentPipeline) = pipelineConstructor.Construct(
		{
		theirSceneGlobals.GetGlobalsLayout(),
		myPresentRenderPass.subpasses[0].inputAttachmentLayout
		}, theirVulkanFramework.GetDevice());


	// COMMANDS
	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		auto [resultBuffer, buffer] = theirVulkanFramework.RequestCommandBuffer(cmdBufferFamily);
		assert(!resultBuffer && "failed creating cmd buffer");
		myCmdBuffers[i] = buffer;
	}

	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		auto resultFence = vkCreateFence(theirVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myCmdBufferFences[i]);
		assert(!resultFence && "failed creating fences");
	}
}

Presenter::~Presenter()
{
	SAFE_DELETE(myPresentShader);

	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		vkDestroyFence(theirVulkanFramework.GetDevice(), myCmdBufferFences[i], nullptr);
	}
}

std::tuple<VkSubmitInfo, VkFence>
Presenter::RecordSubmit(
	uint32_t				swapchainImageIndex,
	VkSemaphore*			waitSemaphores,
	uint32_t				numWaitSemaphores,
	VkPipelineStageFlags*	waitPipelineStages,
	uint32_t				numWaitStages,
	VkSemaphore*			signalSemaphore)
{
	// RECORD
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;

	beginInfo.pInheritanceInfo = nullptr;

	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
	}
	auto resultBegin = vkBeginCommandBuffer(myCmdBuffers[swapchainImageIndex], &beginInfo);

	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	BeginRenderPass(myCmdBuffers[swapchainImageIndex], myPresentRenderPass, swapchainImageIndex, {0,0,w,h});

	vkCmdBindPipeline(myCmdBuffers[swapchainImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, myPresentPipeline.pipeline);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(myCmdBuffers[swapchainImageIndex], myPresentPipeline.layout, 0);
	BindSubpassInputs(myCmdBuffers[swapchainImageIndex], myPresentPipeline.layout, 1, myPresentRenderPass.subpasses[0], swapchainImageIndex);

	// DRAW 
	vkCmdDraw(myCmdBuffers[swapchainImageIndex], 6, 1, 0, 0);

	vkCmdNextSubpass(myCmdBuffers[swapchainImageIndex], VK_SUBPASS_CONTENTS_INLINE);

	VkClearAttachment clearAtt{};
	clearAtt.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	clearAtt.colorAttachment = 0;
	VkClearRect clearRect{};
	clearRect.baseArrayLayer = 0;
	clearRect.layerCount = 1;
	clearRect.rect = {0,0,uint32_t(w),uint32_t(h)};

	vkCmdClearAttachments(myCmdBuffers[swapchainImageIndex], 1, &clearAtt, 1, &clearRect);

	vkCmdEndRenderPass(myCmdBuffers[swapchainImageIndex]);

	auto resultEnd = vkEndCommandBuffer(myCmdBuffers[swapchainImageIndex]);

	if (resultBegin || resultEnd)
	{
		LOG("mesh renderer failed recording");
		return {};
	}

	// FILL IN SUBMISSION
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;

	submitInfo.pWaitDstStageMask = waitPipelineStages;

	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.waitSemaphoreCount = numWaitSemaphores;
	submitInfo.pCommandBuffers = &myCmdBuffers[swapchainImageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphore;
	submitInfo.signalSemaphoreCount = 1;

	return {submitInfo, myCmdBufferFences[swapchainImageIndex]};
}
