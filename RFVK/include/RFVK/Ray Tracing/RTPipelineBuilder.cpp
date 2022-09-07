#include "pch.h"
#include "RTPipelineBuilder.h"

#include "NVRayTracing.h"
#include "RFVK/Shader/Shader.h"

void
RTPipelineBuilder::AddDescriptorSet(
	VkDescriptorSetLayout setLayout)
{
	assert(setLayout != nullptr);
	myDescriptorSets.emplace_back(setLayout);
}

void
RTPipelineBuilder::AddShaderGroup(
	int				index, 
	ShaderGroupType type)
{
	VkRayTracingShaderGroupCreateInfoKHR groupCreateInfo = {};
	groupCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	groupCreateInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	groupCreateInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	groupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	groupCreateInfo.generalShader = VK_SHADER_UNUSED_KHR;
	
	switch (type) {
		case ShaderGroupType::RayGen: 
		case ShaderGroupType::Miss: 
			groupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			groupCreateInfo.generalShader = uint32_t(index);
			break;
		case ShaderGroupType::ClosestHit:
			groupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
			groupCreateInfo.closestHitShader = uint32_t(index);
			break;
	}
	myShaderGroups.emplace_back(groupCreateInfo);
}

void
RTPipelineBuilder::AddShader(
	const std::shared_ptr<class Shader>& shader)
{
	myShader = shader;
}

std::tuple<VkResult, Pipeline>
RTPipelineBuilder::Construct(
	VkDevice										device,
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps)
{
	VkPipelineLayout layout = nullptr;
	VkPipeline pipeline = nullptr;
	
	// LAYOUT
	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = uint32_t(myDescriptorSets.size());
	layoutCreateInfo.pSetLayouts = myDescriptorSets.data();

	{
		auto result = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &layout);
		if (result)
		{
			return {result, {pipeline, layout}};
		}
	}
	// PIPELINE
	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo = {};
	rtPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;

	auto [stages, numStages] = myShader->Bind();
	assert(numStages == myShaderGroups.size() && "mismatch between given shader stages and given shader groups");
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.stageCount = numStages;
	rtPipelineInfo.maxPipelineRayRecursionDepth = rtProps.maxRayRecursionDepth;
	rtPipelineInfo.pGroups = myShaderGroups.data();
	rtPipelineInfo.groupCount = uint32_t(myShaderGroups.size());

	rtPipelineInfo.layout = layout;
	auto result = vkCreateRayTracingPipelines(
		device,
		nullptr,
		nullptr,
		1,
		&rtPipelineInfo,
		nullptr,
		&pipeline);

	return {result, {pipeline, layout}};
}
