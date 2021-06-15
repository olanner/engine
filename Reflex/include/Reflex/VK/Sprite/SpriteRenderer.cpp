#include "pch.h"
#include "SpriteRenderer.h"

#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Image/ImageHandler.h"
#include "Reflex/VK/Pipelines/PipelineBuilder.h"
#include "Reflex/VK/RenderPass/RenderPassFactory.h"
#include "Reflex/VK/Scene/SceneGlobals.h"
#include "Reflex/VK/Shader/Shader.h"
#include "Reflex/VK/Uniform/UniformHandler.h"


SpriteRenderer::SpriteRenderer(
	VulkanFramework&	vulkanFramework,
	SceneGlobals&		sceneGlobals,
	ImageHandler&		imageHandler,
	RenderPassFactory&	renderPassFactory,
	UniformHandler&		uniformHandler,
	QueueFamilyIndex*	firstOwner,
	uint32_t			numOwners,
	uint32_t			cmdBufferFamily)
	: theirVulkanFramework(vulkanFramework)
	, theirSceneGlobals(sceneGlobals)
	, theirImageHandler(imageHandler)
	, theirUniformHandler(uniformHandler)
{
	for (uint32_t ownerIndex = 0; ownerIndex < numOwners; ++ownerIndex)
	{
		myOwners.emplace_back(firstOwner[ownerIndex]);
	}

	//	UNIFORM
	mySpriteInstancesID = theirUniformHandler.RequestUniformBuffer(nullptr, MaxNumSpriteInstances * sizeof SpriteInstance);
	assert(!BAD_ID(mySpriteInstancesID) && "failed creating glyph instance uniform");

	// RENDER PASS
	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	auto rpBuilder = renderPassFactory.GetConstructor();

	rpBuilder.DefineAttachments(
	{
		IntermediateAttachment
	}, {w,h});

	rpBuilder.SetAttachmentNames({"attIntermediate"});
	rpBuilder.AddSubpass().SetColorAttachments({0});
	rpBuilder.AddSubpassDependency(
	{
		VK_SUBPASS_EXTERNAL,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		NULL,
		VK_ACCESS_MEMORY_WRITE_BIT,
		VK_DEPENDENCY_BY_REGION_BIT
	});

	myRenderPass = rpBuilder.Build();

	// PIPELINE
	char shaderPaths[][128]
	{
		"Shaders/sprite_vshader.vert",
		"Shaders/sprite_fshader.frag"
	};
	mySpriteShader = new Shader(shaderPaths,
							  _ARRAYSIZE(shaderPaths),
							  theirVulkanFramework);

	PipelineBuilder pBuilder(1, myRenderPass.renderPass, mySpriteShader);
	pBuilder
		.SetDepthEnabled(false)
		.SetSubpass(0)
		.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetAllBlendStates(GenBlendState::Alpha)
		.DefineViewport({w,h}, {0,0,w,h})
		.DefineVertexInput(nullptr);

	auto [glyphLayout, glyphSet] = theirUniformHandler[mySpriteInstancesID];

	VkResult result;
	std::tie(result, mySpritePipeline) = pBuilder.Construct(
	{
		theirSceneGlobals.GetGlobalsLayout(),
		theirImageHandler.GetSamplerSetLayout(),
		theirImageHandler.GetImageSetLayout(),
		glyphLayout
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

std::tuple<VkSubmitInfo, VkFence>
SpriteRenderer::RecordSubmit(
	uint32_t				swapchainImageIndex,
	VkSemaphore*			waitSemaphores,
	uint32_t				numWaitSemaphores,
	VkPipelineStageFlags*	waitPipelineStages,
	uint32_t				numWaitStages,
	VkSemaphore*			signalSemaphore)
{
	// RECORD
	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
	}

	// ACQUIRE RENDER COMMAND BUFFER
	{
		std::scoped_lock<std::mutex> lock(mySwapMutex);
		std::swap(myRecordIndex, myFreeIndex);
	}

	auto cmdBuffer = myCmdBuffers[swapchainImageIndex];

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;

	beginInfo.pInheritanceInfo = nullptr;

	auto resultBegin = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	BeginRenderPass(cmdBuffer,
					myRenderPass,
					swapchainImageIndex,
					{0,0,w,h});

	vkCmdBindPipeline(myCmdBuffers[swapchainImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, mySpritePipeline.pipeline);

	// UPDATE INSTANCE DATA
	uint32_t numInstances = 0;
	std::array<SpriteInstance, MaxNumSpriteInstances> spriteInstances{};
	for (auto& cmd : myRenderCommands[myRecordIndex])
	{
		spriteInstances[numInstances] = {};
		spriteInstances[numInstances].color = cmd.color;
		spriteInstances[numInstances].pos = cmd.position;
		spriteInstances[numInstances].pivot = cmd.pivot;
		spriteInstances[numInstances].scale = cmd.scale;
		spriteInstances[numInstances].imgArrID = float(cmd.imgArrID);
		spriteInstances[numInstances].imgArrIndex = float(cmd.imgArrIndex);
		numInstances++;
	}
	theirUniformHandler.UpdateUniformData(mySpriteInstancesID, spriteInstances.data());

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, mySpritePipeline.layout, 0);
	theirImageHandler.BindSamplers(cmdBuffer, mySpritePipeline.layout, 1);
	theirImageHandler.BindImages(cmdBuffer, mySpritePipeline.layout, 2);
	theirUniformHandler.BindUniform(mySpriteInstancesID, cmdBuffer, mySpritePipeline.layout, 3);

	// DRAW
	if (numInstances > 0)
	{
		vkCmdDraw(myCmdBuffers[swapchainImageIndex], 6, numInstances, 0, 0);
	}

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

void
SpriteRenderer::BeginPush()
{
	{
		std::scoped_lock<std::mutex> lock(mySwapMutex);
		std::swap(myPushIndex, myFreeIndex);
	}
	myRenderCommands[myPushIndex].clear();
}

void
SpriteRenderer::PushRenderCommand(
	const SpriteRenderCommand& textRenderCommand)
{
	myRenderCommands[myPushIndex].emplace_back(textRenderCommand);
}

void
SpriteRenderer::EndPush()
{
}


