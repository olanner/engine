#include "pch.h"
#include "VKCompile.h"

#include <fstream>
#include <filesystem>

#include "SPIRV/GlslangToSpv.h"
#include "StandAlone/DirStackFileIncluder.h"
#include "StandAlone/ResourceLimits.h"

#ifdef _DEBUG
#pragma comment( lib, "glslang-default-resource-limitsd.lib" )
#pragma comment( lib, "glslangd.lib" )
#pragma comment( lib, "HLSLd.lib" )
#pragma comment( lib, "OGLCompilerd.lib" )
#pragma comment( lib, "OSDependentd.lib" )
#pragma comment( lib, "SPIRVd.lib" )
#pragma comment( lib, "SPVRemapperd.lib" )
#else

#pragma comment(lib, "glslang-default-resource-limits.lib" )
#pragma comment(lib, "glslang.lib" )
#pragma comment(lib, "HLSL.lib" )
#pragma comment(lib, "OGLCompiler.lib" )
#pragma comment(lib, "OSDependent.lib" )
#pragma comment(lib, "SPIRV.lib" )
#pragma comment(lib, "SPVRemapper.lib" )


#endif

bool glslangInit = false;

std::vector<uint32_t>
VKCompile(
	const char*							shaderPath,
	glslang::EShTargetClientVersion		vkVersion,
	glslang::EShTargetLanguageVersion	spvVersion,
	const char*							preAmble)
{
	if (!glslangInit)
	{
		glslang::InitializeProcess();
		glslangInit = true;
	}
	std::vector<uint32_t> spvBin;

	std::ifstream file;
	file.open(shaderPath);
	if (!file.good())
	{
		LOG("failed opening shader file :", shaderPath);
		return spvBin;
	}

	std::string glslBuffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	const char* rawGLSL = glslBuffer.c_str();
	auto ext = std::filesystem::path(shaderPath).extension().string();

	EShLanguage stage;
	if (ext.find(".vert") != std::string::npos)
	{
		stage = EShLangVertex;
	}
	else if (ext.find(".frag") != std::string::npos)
	{
		stage = EShLangFragment;
	}
	else if (ext.find(".rgen") != std::string::npos)
	{
		stage = EShLangRayGenNV;
	}
	else if (ext.find(".rint") != std::string::npos)
	{
		stage = EShLangIntersectNV;
	}
	else if (ext.find(".rahit") != std::string::npos)
	{
		stage = EShLangAnyHitNV;
	}
	else if (ext.find(".rchit") != std::string::npos)
	{
		stage = EShLangClosestHitNV;
	}
	else if (ext.find(".rmiss") != std::string::npos)
	{
		stage = EShLangMissNV;
	}
	else if (ext.find(".rcall") != std::string::npos)
	{
		stage = EShLangCallableNV;;
	}
	else
	{
		LOG("shader stage not supported for :", shaderPath);
		return spvBin;
	}

	glslang::TShader shader(stage);
	std::string ambled(rawGLSL);
	if (preAmble)
	{
		uint32_t pos = ambled.rfind("enable");
		if (pos != std::string::npos && pos < ambled.size())
		{
			ambled.insert(pos + 7, preAmble);
		}
		else
		{
			uint32_t pos = ambled.rfind("require");
			assert(pos != std::string::npos);
			ambled.insert(pos + 8, preAmble);
		}
		const char* rawAmbled = ambled.c_str();
		int len = ambled.length();
		const char* names[]
		{
			""
		};
		shader.setStringsWithLengthsAndNames(&rawAmbled, &len, names, 1);
	}
	else
	{
		shader.setStrings(&rawGLSL, 1);
	}

	auto vkVer = int(VK_VERSION_MAJOR(vkVersion) * 100 + VK_VERSION_MINOR(vkVersion) * 10);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, vkVer);
	shader.setEnvClient(glslang::EShClientVulkan, vkVersion);
	shader.setEnvTarget(glslang::EshTargetSpv, spvVersion);

	EShMessages messages = (EShMessages) (EShMsgSpvRules | EShMsgVulkanRules);


	// PRE PROCESS
	DirStackFileIncluder includer;
	auto folder = std::filesystem::path(shaderPath).parent_path().string();

	std::string preprocessed;

	bool resultPreProcess = shader.preprocess(&glslang::DefaultTBuiltInResource,
											   460,
											   ENoProfile,
											   false,
											   true,
											   messages,
											   &preprocessed,
											   includer);

	if (!resultPreProcess)
	{
		LOG("failed to pre-process :", shaderPath, '\n', shader.getInfoLog(), '\n', shader.getInfoDebugLog());
		return spvBin;
	}

	// PARSE
	bool resultParse = shader.parse(&glslang::DefaultTBuiltInResource, 100, true, messages, includer);
	if (!resultParse)
	{
		LOG("failed to parse :", shaderPath, '\n', shader.getInfoLog(), '\n', shader.getInfoDebugLog());
		return spvBin;
	}

	// LINK
	glslang::TProgram program;
	program.addShader(&shader);
	auto resultLink = program.link(messages);
	if (!resultLink)
	{
		LOG("failed to link :", shaderPath, '\n', shader.getInfoLog(), '\n', shader.getInfoDebugLog());
		return spvBin;
	}

	spv::SpvBuildLogger logger;
	glslang::SpvOptions options;
	glslang::GlslangToSpv(*program.getIntermediate(stage), spvBin, &logger, &options);

	return spvBin;
}
