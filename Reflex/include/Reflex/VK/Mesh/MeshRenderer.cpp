
#include "pch.h"

#include "MeshRenderer.h"
#include "MeshRenderCommand.h"

#include "Reflex/VK/Shader/Shader.h"
#include "MeshHandler.h"
#include "Reflex/VK/Image/ImageHandler.h"
#include "Reflex/VK/Uniform/UniformHandler.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Pipelines/MeshPipeline.h"
#include "Reflex/VK/Pipelines/PipelineBuilder.h"
#include "Reflex/VK/RenderPass/RenderPassFactory.h"
#include "Reflex/VK/Scene/SceneGlobals.h"

MeshRenderer::MeshRenderer(
	VulkanFramework& vulkanFramework,
	UniformHandler& uniformHandler,
	MeshHandler& meshHandler,
	ImageHandler& imageHandler,
	SceneGlobals& sceneGlobals,
	RenderPassFactory& renderPassFactory,
	QueueFamilyIndex cmdBufferFamily)
	: MeshRendererBase(vulkanFramework,
					  uniformHandler,
					  meshHandler,
					  imageHandler,
					  sceneGlobals,
					  cmdBufferFamily)
	, theirRenderPassFactory(renderPassFactory)
	, myInstanceData(new UniformInstances{})
	, myDeferredRenderPass{}

{

	// INSTANCE DATA
	myInstanceUniformID = theirUniformHandler.RequestUniformBuffer(myInstanceData.get(),
																	sizeof UniformInstances);
	assert(!(BAD_ID(myInstanceUniformID)) && "failed creating matrices uniform");


	// RENDER PASS
	auto [w, h] = theirVulkanFramework.GetTargetResolution();

	auto rpBuilder = theirRenderPassFactory.GetConstructor();

	rpBuilder.DefineAttachments(
		{
			VK_FORMAT_R8G8B8A8_SRGB,       //ALBEDO
			VK_FORMAT_R32G32B32A32_SFLOAT, //NORMAL
			VK_FORMAT_R32G32B32A32_SFLOAT, //WPOS
			VK_FORMAT_R8G8B8A8_SRGB,	   //MATERIAL
			VK_FORMAT_D24_UNORM_S8_UINT,   //DEPTH
			IntermediateAttachment,   //INT
		}
	, {w, h});
	rpBuilder.SetAttachmentNames({
		"attAlbedo",
		"attNormal",
		"attPosition",
		"attMaterial",
		"attDepth",
		"attInt",
								 });
	rpBuilder.SetClearValues({
		{.color = {1,0,0,0}},
		{.color = {0,1,0,0}},
		{.color = {0,0,1,0}},
		{.color = {0,1,1,0}},
		{.depthStencil = {1,0}},
							 });

	rpBuilder.AddSubpass()
		.SetColorAttachments({0,1,2,3})
		.SetDepthAttachment(4);

	rpBuilder.AddSubpass()
		.SetColorAttachments({5})
		.SetInputAttachments({0,1,2,3});


	rpBuilder.AddSubpassDependency(
		{
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		});
	rpBuilder.AddSubpassDependency(
		{
			0,
			1,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,
		});

	myDeferredRenderPass = rpBuilder.Build();

	// DEFERRED GEO PIPELINE
	auto globLayout = theirSceneGlobals.GetGlobalsLayout();
	auto [instLayout, instSet] = theirUniformHandler[myInstanceUniformID];

	char shaderPaths[][128]
	{
		"Shaders/base_vshader.vert",
		"Shaders/deferred_geo_fshader.frag"
	};
	myDeferredGeoShader = new Shader(shaderPaths,
									  _ARRAYSIZE(shaderPaths),
									  theirVulkanFramework);

	PipelineBuilder pBuilder(4, myDeferredRenderPass.renderPass, myDeferredGeoShader);
	pBuilder.DefineVertexInput(&Vertex3DInputInfo)
		.DefineViewport({w, h}, {0,0,w,h})
		.SetAllBlendStates(GenBlendState::Disabled);
	VkResult result;
	std::tie(result, myDeferredGeoPipeline) = pBuilder.Construct({
		globLayout,
		theirImageHandler.GetSamplerSetLayout(),
		theirImageHandler.GetImageSetLayout(),
		instLayout}, theirVulkanFramework.GetDevice());

	// DEFERRED LIGHT PIPELINE
	char shaderPathsLight[][128]
	{
		"Shaders/fullscreen_vshader.vert",
		"Shaders/deferred_light_fshader.frag"
	};
	myDeferredGeoShader = new Shader(shaderPathsLight,
									  _ARRAYSIZE(shaderPathsLight),
									  theirVulkanFramework);

	PipelineBuilder pBuilderGeo(1, myDeferredRenderPass.renderPass, myDeferredGeoShader);
	pBuilderGeo.DefineVertexInput(nullptr)
		.DefineViewport({w, h}, {0,0,w,h})
		.SetAllBlendStates(GenBlendState::Disabled)
		.SetDepthEnabled(false)
		.SetSubpass(1);

	std::tie(result, myDeferredLightPipeline) = pBuilderGeo.Construct({
		globLayout,
		theirImageHandler.GetSamplerSetLayout(),
		theirImageHandler.GetImageSetLayout(),
		myDeferredRenderPass.subpasses[1].inputAttachmentLayout}, theirVulkanFramework.GetDevice());
}

MeshRenderer::~MeshRenderer()
{
	SAFE_DELETE(myDeferredGeoShader);
}

std::tuple<VkSubmitInfo, VkFence>
MeshRenderer::RecordSubmit(
	uint32_t				swapchainImageIndex,
	VkSemaphore*			waitSemaphores,
	uint32_t				numWaitSemaphores,
	VkPipelineStageFlags*	waitPipelineStages,
	uint32_t				numWaitStages,
	VkSemaphore*			signalSemaphore
)
{
	// ACQUIRE RENDER COMMAND BUFFER
	auto& assembledWork = myWorkScheduler.AssembleScheduledWork();

	// PROCESS COMMANDS
	std::sort(assembledWork.begin(), assembledWork.end());

	static std::array<std::pair<uint32_t, uint32_t>, MaxNumMeshesLoaded> instanceControl;
	instanceControl = {};

	MeshID currentID = MeshID(INVALID_ID);
	uint32_t index = 0;
	for (auto& cmd : assembledWork)
	{
		if (cmd.id != currentID)
		{
			currentID = cmd.id;
			instanceControl[int(cmd.id)].first = index;
		}
		++instanceControl[int(currentID)].second;

		myInstanceData->instances[index].mat = cmd.transform;
		myInstanceData->instances[index].objID = uint32_t(cmd.id);

		index++;
	}

	theirUniformHandler.UpdateUniformData(myInstanceUniformID, myInstanceData.get());


	// RECORD
	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
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
					 myDeferredRenderPass,
					 swapchainImageIndex,
					 {0,0,w,h});

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, myDeferredGeoPipeline.pipeline);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, myDeferredGeoPipeline.layout, 0);
	theirImageHandler.BindSamplers(cmdBuffer, myDeferredGeoPipeline.layout, 1);
	theirImageHandler.BindImages(swapchainImageIndex, cmdBuffer, myDeferredGeoPipeline.layout, 2);
	theirUniformHandler.BindUniform(myInstanceUniformID, cmdBuffer, myDeferredGeoPipeline.layout, 3);
	
	// MESHES
	for (uint32_t i = 0; i < assembledWork.size();)
	{
		auto& cmd = assembledWork[i];
		auto [first, num] = instanceControl[int(cmd.id)];
		RecordMesh(cmdBuffer,
					theirMeshHandler[cmd.id].geo,
					first,
					num
		);
		i += num;
	}

	vkCmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, myDeferredLightPipeline.pipeline);

	theirSceneGlobals.BindGlobals(cmdBuffer, myDeferredGeoPipeline.layout, 0);
	theirImageHandler.BindSamplers(cmdBuffer, myDeferredGeoPipeline.layout, 1);
	theirImageHandler.BindImages(swapchainImageIndex, cmdBuffer, myDeferredGeoPipeline.layout, 2);

	BindSubpassInputs(cmdBuffer, myDeferredLightPipeline.layout, 3, myDeferredRenderPass.subpasses[1], swapchainImageIndex);

	vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

	vkCmdEndRenderPass(cmdBuffer);

	auto resultEnd = vkEndCommandBuffer(cmdBuffer);

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
