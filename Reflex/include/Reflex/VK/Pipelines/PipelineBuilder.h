#pragma once
#include "Pipeline.h"

enum class GenBlendState
{
	Alpha,

	Disabled
};

class PipelineBuilder
{
public:
	PipelineBuilder(unsigned numColAttachments,
						 VkRenderPass renderPass,
						 class Shader* shaderPtr);
	~PipelineBuilder();

	PipelineBuilder&	DefineVertexInput(const VkPipelineVertexInputStateCreateInfo* inputState);
	PipelineBuilder&	DefineViewport(
							Vec2f resolution, 
							Vec4f scissor);

	PipelineBuilder&	SetPrimitiveTopology(VkPrimitiveTopology primitiveTopology);
	PipelineBuilder&	SetMultisampleCount(unsigned count);

	PipelineBuilder&	SetDepthEnabled(bool enabled);
	PipelineBuilder&	SetDepthState(
							VkCompareOp compareOp, 
							bool		readOnly = false);
	PipelineBuilder&	SetBlendState(
							unsigned		attachmentIndex, 
							GenBlendState	blendState);
	PipelineBuilder&	SetAllBlendStates(GenBlendState blendState);
	PipelineBuilder&	SetSubpass(uint32_t subpassIndex);

	_NODISCARD std::tuple<VkResult, Pipeline>
						Construct(
							std::vector<VkDescriptorSetLayout>&&	descriptorSets, 
							VkDevice								device) const;

private:
	VkRenderPass		myRenderPass;
	class Shader*		myShaderPtr;

	bool				myHasSetVertexInput = false;
	const VkPipelineVertexInputStateCreateInfo* 
						myVertexInputInfoPtr;
	VkPipelineInputAssemblyStateCreateInfo
						myInputAssemblyInfo;

	bool				myHasSetViewport = false;
	VkRect2D			myScissor;
	VkViewport			myViewport;
	VkPipelineViewportStateCreateInfo
						myViewportInfo;

	VkPipelineRasterizationStateCreateInfo
						myRasterInfo;
	VkPipelineMultisampleStateCreateInfo
						myMultisampleInfo;
	VkPipelineDepthStencilStateCreateInfo
						myDepthStencilInfo;
	std::vector<VkPipelineColorBlendAttachmentState>
						myColorAttBlendInfos;
	VkPipelineColorBlendStateCreateInfo
						myBlendInfo;

	uint32_t			mySubpassIndex = 0;

};

