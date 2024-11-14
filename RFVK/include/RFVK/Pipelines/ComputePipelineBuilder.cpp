#include "pch.h"
#include "ComputePipelineBuilder.h"
#include "RFVK/Shader/Shader.h"

void
ComputePipelineBuilder::AddDescriptorSet(
	VkDescriptorSetLayout setLayout)
{
	myDescriptorSets.emplace_back(setLayout);
}

void
ComputePipelineBuilder::AddShader(
	const std::shared_ptr<Shader>& shader)
{
	myShader = shader;
}

std::tuple<VkResult, Pipeline>
ComputePipelineBuilder::Construct(
	VkDevice device) const
{
	Pipeline pipeline = {};
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = uint32_t(myDescriptorSets.size());
	layoutCreateInfo.pSetLayouts = myDescriptorSets.data();

	VkResult result = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipeline.layout);
	if (result)
	{
		return { result, pipeline };
	}

	VkComputePipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = pipeline.layout;
	

	auto boundModules = myShader->Bind();
	assert(boundModules.numModules == 1 && "compute pipeline only accepts one shader module");
	pipelineCreateInfo.stage = boundModules.modules[0];

	result = vkCreateComputePipelines(device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline.pipeline);

	return {result, pipeline};
}
