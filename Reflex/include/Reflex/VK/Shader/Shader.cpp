#include "pch.h"
#include "Shader.h"

#include "VKCompile.h"

#include <fstream>
#include <iostream>
#include <filesystem>
#include "neat/FS/FileUtil.h"
#include "Reflex/VK/VulkanFramework.h"

Shader::Shader(
	const char			(*firstShaderPath)[128],
	uint32_t			numShaderPaths,
	VulkanFramework&	vulkanFramework)
	: theirVulkanFramework(vulkanFramework)
	, myShaderModules{}
	, myNumShaderModules(numShaderPaths)
{
	for (uint32_t i = 0; i < numShaderPaths; ++i)
	{
		auto bin = FetchBinaryData(firstShaderPath[i]);
		assert(!bin.empty() && "shader binary data empty");

		VkShaderModuleCreateInfo vShaderModuleInfo{};
		vShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vShaderModuleInfo.pNext = nullptr;

		vShaderModuleInfo.flags = NULL;
		vShaderModuleInfo.codeSize = bin.size() * 4;
		vShaderModuleInfo.pCode = reinterpret_cast<uint32_t*>(bin.data());

		myShaderModules[i].stage = EvaluateShaderStage(firstShaderPath[i]);

		const auto resultVert = vkCreateShaderModule(theirVulkanFramework.GetDevice(), &vShaderModuleInfo, nullptr, &myShaderModules[i].mod);
		assert(!resultVert && "failed creating shader module");
	}
}


Shader::~Shader()
{
	for (uint32_t i = 0; i < myNumShaderModules; ++i)
	{
		if (myShaderModules[i].mod)
		{
			vkDestroyShaderModule(theirVulkanFramework.GetDevice(), myShaderModules[i].mod, nullptr);
		}
	}
}

BoundModules
Shader::Bind()
{
	std::array<VkPipelineShaderStageCreateInfo, MaxNumShaderModulesPerShader> ret{};
	for (uint32_t i = 0; i < myNumShaderModules; ++i)
	{
		VkPipelineShaderStageCreateInfo stage{};
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.pNext = nullptr;
		stage.flags = NULL;

		stage.stage = myShaderModules[i].stage;
		stage.module = myShaderModules[i].mod;
		stage.pName = "main";

		ret[i] = stage;
	}

	return {ret, myNumShaderModules};
}

std::vector<uint32_t>
Shader::FetchBinaryData(const char* path) const
{
	std::vector<uint32_t> retBin;

	if (!std::filesystem::exists("Shaders/Cached/"))
	{
		std::filesystem::create_directory("Shaders/Cached/");
	}
	std::string cachedPath = "Shaders/Cached/";

	cachedPath.append(std::filesystem::path(path).filename().replace_extension(".spv").string());

	bool recompile = true;
	if (std::filesystem::exists(cachedPath))
	{
		recompile = false;
		if (neat::FileAgeDiff(cachedPath.c_str(), path) < 0)
		{
			recompile = true;
		}
		if (neat::FileAgeDiff(cachedPath.c_str(), "Shaders/common.glsl") < 0)
		{
			recompile = true;
		}
		if (neat::FileAgeDiff(cachedPath.c_str(), "Shaders/rt_common.glsl") < 0)
		{
			recompile = true;
		}
		if (!recompile)
		{
			LOG("using cached shader for", path);
			std::ifstream inBin;
			inBin.open(cachedPath, std::ios::binary | std::ios::ate);
			if (!inBin.good())
				return retBin;

			auto size = inBin.tellg();
			retBin.resize(size / sizeof(uint32_t));

			inBin.seekg(0, std::ios::beg);
			inBin.read((char*) retBin.data(), retBin.size() * 4);
			inBin.close();
			recompile = false;
		}
	}
	if (recompile)
	{
		LOG("compiling shader", path);

		std::string preAmble;
		preAmble.append("#define SET_SAMPLERS_COUNT ").append(std::to_string(MaxNumSamplers)).append("\n");
		preAmble.append("#define SET_TLAS_COUNT ").append(std::to_string(MaxNumInstanceStructures)).append("\n");
		preAmble.append("#define SAMPLED_IMAGE_2D_COUNT ").append(std::to_string(MaxNumImages)).append("\n");
		preAmble.append("#define SAMPLED_IMAGE_2D_ARRAY_COUNT ").append(std::to_string(MaxNumImages)).append("\n");
		preAmble.append("#define SAMPLED_CUBE_COUNT ").append(std::to_string(MaxNumImagesCube)).append("\n");
		preAmble.append("#define STORAGE_IMAGE_COUNT ").append(std::to_string(MaxNumStorageImages)).append("\n");
		preAmble.append("#define MAX_NUM_INSTANCES ").append(std::to_string(MaxNumInstances)).append("\n");
		preAmble.append("#define MAX_NUM_MESHES ").append(std::to_string(MaxNumMeshesLoaded)).append("\n");

		retBin = VKCompile(path, glslang::EShTargetVulkan_1_1, glslang::EShTargetSpv_1_3, preAmble.c_str());
		if (!retBin.empty())
		{
			std::ofstream outBin;
			outBin.open(cachedPath, std::ios::binary);

			auto size = retBin.size() * sizeof(uint32_t);
			outBin.write((const char*) retBin.data(), size);
			outBin.close();
		}
	}

	return retBin;
}

VkShaderStageFlagBits
Shader::EvaluateShaderStage(const char* path)
{
	std::string ext = std::filesystem::path(path).extension().string();
	if (ext.find(".frag") != std::string::npos)
	{
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	if (ext.find(".vert") != std::string::npos)
	{
		return VK_SHADER_STAGE_VERTEX_BIT;
	}
	if (ext.find(".rgen") != std::string::npos)
	{
		return VK_SHADER_STAGE_RAYGEN_BIT_NV;
	}
	if (ext.find(".rint") != std::string::npos)
	{
		return VK_SHADER_STAGE_RAYGEN_BIT_NV;
	}
	if (ext.find(".rahit") != std::string::npos)
	{
		return VK_SHADER_STAGE_ANY_HIT_BIT_NV;
	}
	if (ext.find(".rchit") != std::string::npos)
	{
		return VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
	}
	if (ext.find(".rmiss") != std::string::npos)
	{
		return VK_SHADER_STAGE_MISS_BIT_NV;
	}
	if (ext.find(".rcall") != std::string::npos)
	{
		return VK_SHADER_STAGE_CALLABLE_BIT_NV;
	}
	assert("undefined shader stage");
	return VkShaderStageFlagBits(NULL);
}

