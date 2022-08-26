#pragma once

// WINAPI
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>

#include <concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include <concurrent_unordered_map.h>

template<typename key, typename value>
using conc_map = concurrency::concurrent_unordered_map<key, value>;
template<typename type>
using conc_queue = concurrency::concurrent_queue<type>;

// STL
#include <functional>
#include <assert.h>
#include <fstream>
#include <array>
#include <iostream>
#include <string>
#include <unordered_set>
#include <mutex>
#include <queue>
#include <memory>
#include <filesystem>
#include <shared_mutex>
#include <string>
#include <semaphore>

using shared_lock = std::shared_lock<std::shared_mutex>;
template<std::ptrdiff_t max>
using shared_semaphore = std::shared_ptr<std::counting_semaphore<max>>;

// LOCAL
#include "RFVK/Misc/Macros.h"
#include "RFVK/Misc/Aliases.h"
#include "RFVK/Misc/Constants.h"
#include "RFVK/Misc/Helpers.h"
#include "RFVK/Misc/Identities.h"

// VULKAN
#include "vulkan/vulkan.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_win32.h"
#include "vulkan/vk_layer.h"
// GLM
#include "glm/glm.hpp"
#include "glm/ext.hpp"

using Vec2f = glm::vec2;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Mat3f = glm::mat3x3;
using Mat4f = glm::mat4x4;
using Mat4nv = glm::mat3x4;
using Mat4rt = glm::mat3x4;

using Vec2uc = glm::u8vec2;
using Vec3uc = glm::u8vec3;
using Vec4uc = glm::u8vec4;

using Vec2ui = glm::vec<2, uint32_t>;

// NEAT
#include "neat/Containers/static_vector.h"
#include "neat/Image/ImageReader.h"
#include "neat/Misc/IDKeeper.h"
#include "neat/General/Thread.h"

// rapidjson
#include "rapidjson/document.h"
#include "rapidjson/rapidjson.h"