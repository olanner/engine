#pragma once

#define ENABLE_OPT

#include <vector>
#include "glslang/public/ShaderLang.h"

std::vector<uint32_t> VKCompile(
						const char *						shaderPath,
                        glslang::EShTargetClientVersion		vkVersion,
                        glslang::EShTargetLanguageVersion	spvVersion, 
                        const char*							preAmble = nullptr );