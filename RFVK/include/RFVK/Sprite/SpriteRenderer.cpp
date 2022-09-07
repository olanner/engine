#include "pch.h"
#include "SpriteRenderer.h"

#include "RFVK/VulkanFramework.h"
#include "RFVK/Image/ImageHandler.h"
#include "RFVK/Pipelines/PipelineBuilder.h"
#include "RFVK/RenderPass/RenderPassFactory.h"
#include "RFVK/Scene/SceneGlobals.h"
#include "RFVK/Shader/Shader.h"
#include "RFVK/Uniform/UniformHandler.h"


SpriteRenderer::SpriteRenderer(
	VulkanFramework&	vulkanFramework,
	SceneGlobals&		sceneGlobals,
	ImageHandler&		imageHandler,
	RenderPassFactory&	renderPassFactory,
	UniformHandler&		uniformHandler,
	QueueFamilyIndices	familyIndices)
	: theirVulkanFramework(vulkanFramework)
	, theirSceneGlobals(sceneGlobals)
	, theirImageHandler(imageHandler)
	, theirUniformHandler(uniformHandler)
{
	myWaitStages.fill(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
	myOwners = {
		familyIndices[QUEUE_FAMILY_TRANSFER],
		familyIndices[QUEUE_FAMILY_GRAPHICS],
	};

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
		auto [resultBuffer, buffer] = theirVulkanFramework.RequestCommandBuffer(familyIndices[QUEUE_FAMILY_TRANSFER]);
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

neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
SpriteRenderer::RecordSubmit(
	uint32_t	swapchainImageIndex,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&
	waitSemaphores,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&
	signalSemaphores)
{
	// RECORD
	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
	}

	// ACQUIRE RENDER COMMAND BUFFER
	const auto& assembledWork = myWorkScheduler.AssembleScheduledWork();

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
	for (auto& cmd : assembledWork)
	{
		if (BAD_ID(cmd.imgArrID) || int(cmd.imgArrID) > MaxNumImages)
		{
			continue;
		}
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
	theirImageHandler.BindImages(swapchainImageIndex, cmdBuffer, mySpritePipeline.layout, 2);
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

	submitInfo.pWaitDstStageMask = myWaitStages.data();

	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pCommandBuffers = &myCmdBuffers[swapchainImageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores.data();
	submitInfo.signalSemaphoreCount = signalSemaphores.size();

	return {{myCmdBufferFences[swapchainImageIndex], submitInfo, VK_QUEUE_GRAPHICS_BIT}};
}

std::array<VkFence, NumSwapchainImages>
SpriteRenderer::GetFences()
{
    return myCmdBufferFences;
}

std::vector<rflx::Features> SpriteRenderer::GetImplementedFeatures() const
{
    return {rflx::Features::FEATURE_SPRITES};
}

