#include "pch.h"
#include "DeferredGeoRenderer.h"

#include "RFVK/VulkanFramework.h"
#include "RFVK/Geometry/Vertex3D.h"
#include "RFVK/Image/ImageHandler.h"
#include "RFVK/Mesh/Mesh.h"
#include "RFVK/Mesh/MeshHandler.h"
#include "RFVK/Pipelines/PipelineBuilder.h"
#include "RFVK/RenderPass/RenderPassFactory.h"
#include "RFVK/Scene/SceneGlobals.h"
#include "RFVK/Shader/Shader.h"
#include "RFVK/Uniform/UniformHandler.h"

DeferredGeoRenderer::DeferredGeoRenderer(
	VulkanFramework& vulkanFramework, 
	UniformHandler& uniformHandler, 
	MeshHandler& meshHandler, 
	ImageHandler& imageHandler, 
	SceneGlobals& sceneGlobals, 
	RenderPassFactory& renderPassFactory, 
	GBuffer gBuffer, 
	QueueFamilyIndices familyIndices)
	: theirVulkanFramework(vulkanFramework)
	, theirUniformHandler(uniformHandler)
	, theirMeshHandler(meshHandler)
	, theirImageHandler(imageHandler)
	, theirSceneGlobals(sceneGlobals)
	, theirRenderPassFactory(renderPassFactory)
{
	// RENDER PASS
	auto [sw, sh] = theirVulkanFramework.GetTargetResolution();
	
	auto rpBuilder = renderPassFactory.GetConstructor();
	rpBuilder.DefineAttachments(
		{
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_FORMAT_D24_UNORM_S8_UINT
		}, {sw, sh});
	rpBuilder.SetImageViews(0, gBuffer.albedo);
	rpBuilder.SetImageViews(1, gBuffer.position);
	rpBuilder.SetImageViews(2, gBuffer.normal);
	rpBuilder.SetImageViews(3, gBuffer.material);
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
	rpBuilder.AddSubpassDependency(
		{
			VK_SUBPASS_EXTERNAL,
			0,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT
		});
	rpBuilder.AddSubpassDependency(
		{
			0,
			VK_SUBPASS_EXTERNAL,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
			VK_ACCESS_SHADER_READ_BIT,
			VK_DEPENDENCY_BY_REGION_BIT
		});
	rpBuilder.EditAttachmentDescription(0).finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	rpBuilder.EditAttachmentDescription(1).finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	rpBuilder.EditAttachmentDescription(2).finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	rpBuilder.EditAttachmentDescription(3).finalLayout = VK_IMAGE_LAYOUT_GENERAL;
	
	myDeferredRenderPass = rpBuilder.Build();
	renderPassFactory.RegisterRenderPass(myDeferredRenderPass);

	// PIPELINE
	myInstanceData = std::make_unique<UniformInstances>();
	myInstanceUniformID =  theirUniformHandler.RequestUniformBuffer(nullptr, sizeof UniformInstances);
	
	char shaderPaths[][128]
	{
		"Shaders/base_vshader.vert",
		"Shaders/deferred_geo_fshader.frag"
	};
	myDeferredGeoShader = std::make_shared<Shader>(shaderPaths,
		_ARRAYSIZE(shaderPaths),
		theirVulkanFramework);
	PipelineBuilder pBuilder(4, myDeferredRenderPass.renderPass, myDeferredGeoShader.get());
	pBuilder.DefineVertexInput(&Vertex3DInputInfo)
		.DefineViewport({sw, sh}, {0,0,sw,sh})
		.SetAllBlendStates(GenBlendState::Disabled);

	auto [instLayout, instSet] = theirUniformHandler[myInstanceUniformID];
	VkResult result;
	std::tie(result, myDeferredGeoPipeline) = pBuilder.Construct({
		sceneGlobals.GetGlobalsLayout(),
		theirImageHandler.GetSamplerSetLayout(),
		theirImageHandler.GetImageSetLayout(),
		instLayout}, theirVulkanFramework.GetDevice());
	
}

DeferredGeoRenderer::~DeferredGeoRenderer()
{
	vkDestroyPipeline(
		theirVulkanFramework.GetDevice(),
		myDeferredGeoPipeline.pipeline,
		nullptr);
	vkDestroyPipelineLayout(
		theirVulkanFramework.GetDevice(),
		myDeferredGeoPipeline.layout,
		nullptr);
}

void
DeferredGeoRenderer::Record(
	int												swapchainIndex,
	VkCommandBuffer									cmdBuffer,
	const MeshRenderSchedule::AssembledWorkType&	assembledWork)
{
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
	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	BeginRenderPass(cmdBuffer,
		myDeferredRenderPass,
		swapchainIndex,
		{0,0,w,h});

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, myDeferredGeoPipeline.pipeline);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, myDeferredGeoPipeline.layout, 0);
	theirImageHandler.BindSamplers(cmdBuffer, myDeferredGeoPipeline.layout, 1);
	theirImageHandler.BindImages(swapchainIndex, cmdBuffer, myDeferredGeoPipeline.layout, 2);
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
	
	vkCmdEndRenderPass(cmdBuffer);
}
