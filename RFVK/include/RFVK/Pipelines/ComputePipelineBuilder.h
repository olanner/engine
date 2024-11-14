#pragma once
#include "Pipeline.h"

class ComputePipelineBuilder
{
public:
	void							AddDescriptorSet(
										VkDescriptorSetLayout setLayout);
	void							AddShader(
										const std::shared_ptr<class Shader>& shader);
	std::tuple<VkResult, Pipeline>	Construct(VkDevice device) const;
private:
	std::vector<VkDescriptorSetLayout>	myDescriptorSets;
	std::shared_ptr<class Shader> 		myShader;
};
