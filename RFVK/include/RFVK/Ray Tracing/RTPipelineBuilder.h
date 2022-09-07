#pragma once

#include "RFVK/Pipelines/Pipeline.h"

enum class ShaderGroupType
{
	RayGen,
	Miss,
	ClosestHit
};

class RTPipelineBuilder
{
public:
	void							AddDescriptorSet(VkDescriptorSetLayout setLayout);
	void							AddShaderGroup(
										int				index, 
										ShaderGroupType type);
	void							AddShader(const std::shared_ptr<class Shader>& shader);
	std::tuple<VkResult, Pipeline>	Construct(
										VkDevice device,
										VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps);

private:
	std::vector<VkDescriptorSetLayout>		myDescriptorSets;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR>
											myShaderGroups;
	std::shared_ptr<Shader>			myShader;
	
};
