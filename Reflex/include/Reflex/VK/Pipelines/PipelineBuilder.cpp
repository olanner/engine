#include "pch.h"
#include "PipelineBuilder.h"

#include "GenBlendStates.h"
#include "Reflex/VK/Shader/Shader.h"

PipelineBuilder::PipelineBuilder(
	unsigned		numColAttachments, 
	VkRenderPass	renderPass, 
	Shader*			shaderPtr)
	: myRenderPass(renderPass)
	, myShaderPtr(shaderPtr)
	, myInputAssemblyInfo{}
	, myVertexInputInfoPtr(nullptr)
	, myRasterInfo{}
	, myMultisampleInfo{}
	, myDepthStencilInfo{}
	, myBlendInfo{}

{
	myInputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;

	myInputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
	myInputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	myRasterInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	myRasterInfo.depthClampEnable = VK_FALSE;
	myRasterInfo.rasterizerDiscardEnable = VK_FALSE;
	myRasterInfo.polygonMode = VK_POLYGON_MODE_FILL;
	myRasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	myRasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	myRasterInfo.depthBiasEnable = VK_FALSE;
	myRasterInfo.lineWidth = 1.f;

	myMultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

	myMultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	myMultisampleInfo.sampleShadingEnable = VK_FALSE;

	myDepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	myDepthStencilInfo.pNext = nullptr;
	myDepthStencilInfo.flags = NULL;

	myDepthStencilInfo.depthTestEnable = VK_TRUE;
	myDepthStencilInfo.depthWriteEnable = VK_TRUE;
	myDepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	myDepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
	myDepthStencilInfo.stencilTestEnable = VK_FALSE;
	myDepthStencilInfo.minDepthBounds = 0.f;
	myDepthStencilInfo.maxDepthBounds = 1.f;

	myColorAttBlendInfos.resize(numColAttachments, {
		VK_TRUE,
		VK_BLEND_FACTOR_SRC_ALPHA,
		VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		VK_BLEND_OP_ADD,
		VK_BLEND_FACTOR_ONE,
		VK_BLEND_FACTOR_ZERO,
		VK_BLEND_OP_ADD,
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT});

	myBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

	myBlendInfo.logicOpEnable = VK_FALSE;
	myBlendInfo.attachmentCount = numColAttachments;
	myBlendInfo.pAttachments = myColorAttBlendInfos.data();
}


PipelineBuilder::~PipelineBuilder()
{
}

VkPipelineVertexInputStateCreateInfo
gNullState
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	.vertexBindingDescriptionCount = 0,
	.vertexAttributeDescriptionCount = 0,
};

PipelineBuilder& 
PipelineBuilder::DefineVertexInput(const VkPipelineVertexInputStateCreateInfo* inputState)
{
	myHasSetVertexInput = true;

	if (!inputState)
	{
		myVertexInputInfoPtr = &gNullState;
	}
	else
	{
		myVertexInputInfoPtr = (inputState);
	}


	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetPrimitiveTopology(VkPrimitiveTopology primitiveTopology)
{
	myInputAssemblyInfo.topology = primitiveTopology;

	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetMultisampleCount(unsigned count)
{
	if (count == 1)
	{
		myMultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		myMultisampleInfo.sampleShadingEnable = VK_FALSE;
	}
	else
	{
		myMultisampleInfo.rasterizationSamples = VkSampleCountFlagBits(count);
		myMultisampleInfo.sampleShadingEnable = VK_TRUE;
	}

	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetDepthEnabled(bool enabled)
{
	myDepthStencilInfo.depthTestEnable = enabled;

	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetDepthState(
	VkCompareOp compareOp, 
	bool		readOnly)
{
	myDepthStencilInfo.depthCompareOp = compareOp;
	myDepthStencilInfo.depthWriteEnable = !readOnly;

	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetBlendState(
	unsigned		attachmentIndex, 
	GenBlendState	blendState)
{
	switch (blendState)
	{
		case GenBlendState::Alpha:
			myColorAttBlendInfos[attachmentIndex] = AlphaBlending;
			break;
		case GenBlendState::Disabled:
			myColorAttBlendInfos[attachmentIndex] = DisabledBlending;
			break;
		default: assert(false && "invalid blend state");
	}

	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetAllBlendStates(GenBlendState blendState)
{
	for (uint32_t attachmentIndex = 0; attachmentIndex < myColorAttBlendInfos.size(); ++attachmentIndex)
	{
		SetBlendState(attachmentIndex, blendState);
	}

	return *this;
}

PipelineBuilder& 
PipelineBuilder::SetSubpass(uint32_t subpassIndex)
{
	mySubpassIndex = subpassIndex;

	return *this;
}

PipelineBuilder& 
PipelineBuilder::DefineViewport(
	Vec2f resolution, 
	Vec4f scissor)
{
	myHasSetViewport = true;

	myViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	myViewportInfo.pNext = nullptr;
	myViewportInfo.flags = NULL;

	myScissor.offset = {0, 0};
	myScissor.extent = {uint32_t(resolution.x), uint32_t(resolution.y)};

	myViewportInfo.scissorCount = 1;
	myViewportInfo.pScissors = &myScissor;

	myViewport.width = resolution.x;
	myViewport.height = -float(resolution.y);
	myViewport.x = 0;
	myViewport.y = resolution.y;
	myViewport.minDepth = 0.f;
	myViewport.maxDepth = 1.f;

	myViewportInfo.viewportCount = 1;
	myViewportInfo.pViewports = &myViewport;

	return *this;
}

std::tuple<VkResult, Pipeline>
PipelineBuilder::Construct(
	std::vector<VkDescriptorSetLayout>&&	descriptorSets, 
	VkDevice								device) const
{
	Pipeline retPipeline{};

	if (!myHasSetViewport)
	{
		LOG("failed constructing pipeline, viewport not set");
	}
	if (!myHasSetVertexInput)
	{
		LOG("failed constructing pipeline, vertex input not set");
	}

	// PIPELINE LAYOUT
	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	layoutInfo.pSetLayouts = descriptorSets.data();
	layoutInfo.setLayoutCount = descriptorSets.size();

	auto result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &retPipeline.layout);
	if (result)
	{
		LOG("failed creating pipeline layout");
		return {result, retPipeline};
	}

	// PIPELINE
	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

	pipelineInfo.renderPass = myRenderPass;
	pipelineInfo.layout = retPipeline.layout;

	auto [stages, numStages] = myShaderPtr->Bind();
	pipelineInfo.pStages = stages.data();
	pipelineInfo.stageCount = numStages;

	pipelineInfo.pVertexInputState = myVertexInputInfoPtr;
	pipelineInfo.pInputAssemblyState = &myInputAssemblyInfo;
	pipelineInfo.pViewportState = &myViewportInfo;
	pipelineInfo.pRasterizationState = &myRasterInfo;
	pipelineInfo.pMultisampleState = &myMultisampleInfo;
	pipelineInfo.pDepthStencilState = &myDepthStencilInfo;
	pipelineInfo.pColorBlendState = &myBlendInfo;

	pipelineInfo.subpass = mySubpassIndex;

	result = vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &retPipeline.pipeline);
	if (result)
	{
		LOG("failed creating pipeline");
		return {result, retPipeline};
	}

	return {result, retPipeline};
}
