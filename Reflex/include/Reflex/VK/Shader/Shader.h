
#pragma once

struct ShaderModule
{
	VkShaderModule mod;
	VkShaderStageFlagBits stage;
};

struct BoundModules
{
	std::array<VkPipelineShaderStageCreateInfo, MaxNumShaderModulesPerShader>	modules;
	uint32_t																	numModules;
};

class Shader
{
public:
															Shader(
																const char(*firstShaderPath)[128],
																uint32_t numShaderPaths,
																class VulkanFramework& vulkanFramework);
															~Shader();

	BoundModules											Bind();

private:
	std::vector<uint32_t>									FetchBinaryData(const char* path) const;
	static VkShaderStageFlagBits							EvaluateShaderStage(const char* path);

	VulkanFramework&										theirVulkanFramework;

	std::array<ShaderModule, MaxNumShaderModulesPerShader>	myShaderModules;
	uint32_t												myNumShaderModules;

};