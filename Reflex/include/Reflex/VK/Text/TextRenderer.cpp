#include "pch.h"
#include "TextRenderer.h"

#include "FontHandler.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Image/ImageHandler.h"
#include "Reflex/VK/Pipelines/PipelineBuilder.h"
#include "Reflex/VK/RenderPass/RenderPassFactory.h"
#include "Reflex/VK/Scene/SceneGlobals.h"
#include "Reflex/VK/Shader/Shader.h"
#include "Reflex/VK/Uniform/UniformHandler.h"


TextRenderer::TextRenderer(
	VulkanFramework&	vulkanFramework,
	SceneGlobals&		sceneGlobals,
	FontHandler&		fontHandler,
	RenderPassFactory&	renderPassFactory,
	UniformHandler&		uniformHandler,
	QueueFamilyIndex*	firstOwner,
	uint32_t			numOwners,
	uint32_t			cmdBufferFamily)
	: theirVulkanFramework(vulkanFramework)
	, theirSceneGlobals(sceneGlobals)
	, theirFontHandler(fontHandler)
	, theirUniformHandler(uniformHandler)
{
	for (uint32_t ownerIndex = 0; ownerIndex < numOwners; ++ownerIndex)
	{
		myOwners.emplace_back(firstOwner[ownerIndex]);
	}

	for (char c = FirstFontGlyph; c <= LastFontGlyph; ++c)
	{
		myCharToGlyphIndex[c] = c - FirstFontGlyph;
	}
	theirFontHandler.AddFont("gadugi.ttf");

	//	UNIFORM
	myGlyphInstancesID = theirUniformHandler.RequestUniformBuffer(nullptr, MaxNumGlyphInstances * sizeof GlyphInstance);
	assert(!BAD_ID(myGlyphInstancesID) && "failed creating glyph instance uniform");

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
		"Shaders/text_vshader.vert",
		"Shaders/text_fshader.frag"
	};
	myTextShader = new Shader(shaderPaths,
							  _ARRAYSIZE(shaderPaths),
							  theirVulkanFramework);

	PipelineBuilder pBuilder(1, myRenderPass.renderPass, myTextShader);
	pBuilder
		.SetDepthEnabled(false)
		.SetSubpass(0)
		.SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
		.SetAllBlendStates(GenBlendState::Alpha)
		.DefineViewport({w,h}, {0,0,w,h})
		.DefineVertexInput(nullptr);

	auto [glyphLayout, glyphSet] = theirUniformHandler[myGlyphInstancesID];

	VkResult result;
	std::tie(result, myTextPipeline) = pBuilder.Construct(
	{
		theirSceneGlobals.GetGlobalsLayout(),
		theirFontHandler.GetFontArraySetLayout(),
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

	int val = 0;


}



std::tuple<VkSubmitInfo, VkFence>
TextRenderer::RecordSubmit(
	uint32_t					swapchainImageIndex,
	VkSemaphore* waitSemaphores,
	uint32_t					numWaitSemaphores,
	VkPipelineStageFlags* waitPipelineStages,
	uint32_t					numWaitStages,
VkSemaphore* signalSemaphore)
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

	vkCmdBindPipeline(myCmdBuffers[swapchainImageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, myTextPipeline.pipeline);

	// UPDATE INSTANCE DATA

	uint32_t numGlyphs = 0;
	{
		uint32_t instanceIndex = 0;
		std::array<GlyphInstance, MaxNumGlyphInstances> instances{};
		auto[tw, th] = theirVulkanFramework.GetTargetResolution();
		float ratio = th / tw;
		for (auto&& cmd : myRenderCommands[myRecordIndex])
		{
			float colOffset = 0;
			float rowOffset = 0;
			for (int charIndex = 0; charIndex < cmd.numCharacters; ++charIndex)
			{
				if (instanceIndex == MaxNumGlyphInstances - 1)
				{
					break;
				}
				if (cmd.text[charIndex] == '\n')
				{
					colOffset = 0;
					rowOffset += cmd.scale;
					continue;
				}

				GlyphInstance instance{};
				instance.glyphIndex = myCharToGlyphIndex[cmd.text[charIndex]];
				instance.fontID = float(cmd.fontID);

				GlyphMetrics metrics = theirFontHandler.GetGlyphMetrics(cmd.fontID, instance.glyphIndex);
				metrics.xStride *= cmd.scale;
				metrics.yOffset *= cmd.scale;
				instance.pos = {cmd.position.x + colOffset * ratio, cmd.position.y + metrics.yOffset + rowOffset};

				instance.pivot = {0, -1};
				instance.color = cmd.color;
				instance.scale = cmd.scale;

				colOffset += metrics.xStride;

				instances[instanceIndex++] = instance;
			}
		}

		numGlyphs = instanceIndex + 1;
		theirUniformHandler.UpdateUniformData(myGlyphInstancesID, instances.data());
	}

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, myTextPipeline.layout, 0);
	theirFontHandler.BindFonts(cmdBuffer, myTextPipeline.layout, 1);
	theirUniformHandler.BindUniform(myGlyphInstancesID,
									cmdBuffer,
									myTextPipeline.layout,
									2);

	// DRAW 
	vkCmdDraw(myCmdBuffers[swapchainImageIndex], 6, numGlyphs, 0, 0);

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

void TextRenderer::BeginPush()
{
	{
		std::scoped_lock<std::mutex> lock(mySwapMutex);
		std::swap(myPushIndex, myFreeIndex);
	}
	myRenderCommands[myPushIndex].clear();
}

void TextRenderer::PushRenderCommand(const TextRenderCommand& textRenderCommand)
{
	myRenderCommands[myPushIndex].emplace_back(textRenderCommand);
}

void TextRenderer::EndPush()
{
}


