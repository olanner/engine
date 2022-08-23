#include "pch.h"
#include "VulkanFramework.h"
#include "Ray Tracing/NVRayTracing.h"
#include "Debug/DebugUtils.h"
#include <string>

#pragma comment (lib, "vulkan-1.lib")
#pragma comment (lib, "VkLayer_utils.lib")

VulkanFramework::VulkanFramework()
{
	myClearColor.color = {.1,.1,.133,1};
	myDepthClearColor.depthStencil = {1.f,0};
}

VulkanFramework::~VulkanFramework()
{
	for (auto& [queueFamilyIndex, poolBuffers] : myCmdPoolsAndBuffers)
	{
		auto& [pool, cmdBuffers] = poolBuffers;
		vkFreeCommandBuffers(myDevice, pool, static_cast<int>(cmdBuffers.size()), cmdBuffers.data());
		vkDestroyCommandPool(myDevice, pool, nullptr);
	}
	for (auto& imageView : mySwapchainImageViews)
	{
		vkDestroyImageView(myDevice, imageView, nullptr);
	}
	vkDestroySwapchainKHR(myDevice, mySwapchain, nullptr);
	vkDestroySurfaceKHR(myInstance, mySurface, nullptr);

	vkDestroyRenderPass(myDevice, myForwardPass, nullptr);
	for (int i = 0; i < myNumPipelines; ++i)
	{
		auto [pipeline, layout] = myPipelines[i];
		vkDestroyPipelineLayout(myDevice, layout, nullptr);
		vkDestroyPipeline(myDevice, pipeline, nullptr);
	}

	for (auto& fBuffer : myFrameBuffers)
	{
		vkDestroyFramebuffer(myDevice, fBuffer, nullptr);
	}
	vkDestroyImageView(myDevice, myDepthImageView, nullptr);
	vkDestroyImage(myDevice, myDepthImage, nullptr);
	vkFreeMemory(myDevice, myDepthMemory, nullptr);

	if (gUseDebugLayers)
	{
		vkDestroyDebugUtilsMessenger(myInstance, myDebugMessenger, nullptr);
	}

	vkDeviceWaitIdle(myDevice);
	vkDestroyDevice(myDevice, nullptr);
	vkDestroyInstance(myInstance, nullptr);
}

VkResult
VulkanFramework::Init(
	neat::ThreadID	threadID,
	void*			hWND,
	const Vec2ui&	windowRes,
	bool			useDebugLayers)
{
	myMainThread = threadID;
	
	LOG("Debug Layers", useDebugLayers ? "ON" : "OFF");
	gUseDebugLayers = useDebugLayers;

	VK_FALLTHROUGH(InitInstance());
	VK_FALLTHROUGH(InitDebugMessenger());
	VK_FALLTHROUGH(EnumeratePhysDevices());
	VK_FALLTHROUGH(EnumerateQueueFamilies());
	VK_FALLTHROUGH(InitDevice());
	VK_FALLTHROUGH(InitCmdPoolAndBuffer());
	VK_FALLTHROUGH(InitSurface(hWND));
	VK_FALLTHROUGH(InitSwapchain(windowRes));
	VK_FALLTHROUGH(InitSwapchainImageViews());
	VK_FALLTHROUGH(InitRenderPasses());
	VK_FALLTHROUGH(InitViewport(windowRes));
	VK_FALLTHROUGH(InitDepthImage(windowRes));
	VK_FALLTHROUGH(InitFramebuffers(windowRes));

	vkBindAccelerationStructureMemory = (decltype(vkBindAccelerationStructureMemoryNV)*) vkGetDeviceProcAddr(myDevice, "vkBindAccelerationStructureMemoryNV");
	vkCmdBuildAccelerationStructure = (decltype(vkCmdBuildAccelerationStructureNV)*) vkGetDeviceProcAddr(myDevice, "vkCmdBuildAccelerationStructureNV");
	vkCmdCopyAccelerationStructure = (decltype(vkCmdCopyAccelerationStructureNV)*) vkGetDeviceProcAddr(myDevice, "vkCmdCopyAccelerationStructureNV");
	vkCmdTraceRays = (decltype(vkCmdTraceRaysNV)*) vkGetDeviceProcAddr(myDevice, "vkCmdTraceRaysNV");
	vkCmdWriteAccelerationStructuresProperties = (decltype(vkCmdWriteAccelerationStructuresPropertiesNV)*) vkGetDeviceProcAddr(myDevice, "vkCmdWriteAccelerationStructuresPropertiesNV");
	vkCompileDeferred = (decltype(vkCompileDeferredNV)*) vkGetDeviceProcAddr(myDevice, "vkCompileDeferredNV");
	vkCreateAccelerationStructure = (decltype(vkCreateAccelerationStructureNV)*) vkGetDeviceProcAddr(myDevice, "vkCreateAccelerationStructureNV");
	vkCreateRayTracingPipelines = (decltype(vkCreateRayTracingPipelinesNV)*) vkGetDeviceProcAddr(myDevice, "vkCreateRayTracingPipelinesNV");
	vkDestroyAccelerationStructure = (decltype(vkDestroyAccelerationStructureNV)*) vkGetDeviceProcAddr(myDevice, "vkDestroyAccelerationStructureNV");
	vkGetAccelerationStructureHandle = (decltype(vkGetAccelerationStructureHandleNV)*) vkGetDeviceProcAddr(myDevice, "vkGetAccelerationStructureHandleNV");
	vkGetAccelerationStructureMemoryRequirements = (decltype(vkGetAccelerationStructureMemoryRequirementsNV)*) vkGetDeviceProcAddr(myDevice, "vkGetAccelerationStructureMemoryRequirementsNV");
	vkGetRayTracingShaderGroupHandles = (decltype(vkGetRayTracingShaderGroupHandlesNV)*) vkGetDeviceProcAddr(myDevice, "vkGetRayTracingShaderGroupHandlesNV");

	if (gUseDebugLayers)
	{
		vkSetDebugUtilsObjectName = (decltype(vkSetDebugUtilsObjectNameEXT)*) vkGetInstanceProcAddr(myInstance, "vkSetDebugUtilsObjectNameEXT");
		vkDestroyDebugUtilsMessenger = (decltype(vkDestroyDebugUtilsMessengerEXT)*) vkGetInstanceProcAddr(myInstance, "vkDestroyDebugUtilsMessengerEXT");
	}

	return VK_SUCCESS;
}

std::tuple<VkResult, VkQueue, QueueFamilyIndex>
VulkanFramework::RequestQueue(VkQueueFlagBits queueType)
{
	VkQueue retQueue = nullptr;
	if (myQueueFlagsIndices.find(queueType) == myQueueFlagsIndices.end())
	{
		return {VK_ERROR_FEATURE_NOT_PRESENT, nullptr, -1};
	}
	QueueFamilyIndex chosenQueueFamily = -1;
	int queueIndex = -1;
	for (auto& queueFamilyIndex : myQueueFlagsIndices[queueType])
	{
		if (!myQueuesLeft[queueFamilyIndex])
		{
			continue;
		}
		chosenQueueFamily = queueFamilyIndex;
		queueIndex = --myQueuesLeft[queueFamilyIndex];
		vkGetDeviceQueue(myDevice, queueFamilyIndex, queueIndex, &retQueue);
		break;
	}
	if (!retQueue)
	{
		return {VK_ERROR_FEATURE_NOT_PRESENT, nullptr, -1};
	}
	return {VK_SUCCESS, retQueue, chosenQueueFamily};
}

std::tuple<VkResult, VkCommandBuffer>
VulkanFramework::RequestCommandBuffer(QueueFamilyIndex index)
{
	auto& [pool, cmdBuffers] = myCmdPoolsAndBuffers[index];

	// BUFFER INFO 
	VkCommandBufferAllocateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferInfo.pNext = nullptr;

	bufferInfo.commandPool = pool;
	bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferInfo.commandBufferCount = 1;

	// CREATE BUFFER
	VkCommandBuffer retBuffer = nullptr;
	auto resultBuffer = vkAllocateCommandBuffers(myDevice, &bufferInfo, &retBuffer);
	if (resultBuffer)
	{
		return {VK_ERROR_OUT_OF_POOL_MEMORY, retBuffer};
	}
	if (!retBuffer)
	{
		return {VK_ERROR_OUT_OF_POOL_MEMORY, retBuffer};
	}
	cmdBuffers.emplace_back(retBuffer);
	return {resultBuffer, retBuffer};
}

std::tuple<VkResult, VkPipeline, VkPipelineLayout>
VulkanFramework::CreatePipeline(
	const VkGraphicsPipelineCreateInfo& pipelineCreateInfo,
	const VkPipelineLayoutCreateInfo&	layoutCreateInfo)
{
	VkPipelineLayout layout;
	auto resultLayout = vkCreatePipelineLayout(myDevice,
												&layoutCreateInfo,
												nullptr,
												&layout);
	if (resultLayout)
	{
		return {resultLayout, nullptr, nullptr};
	}

	auto pipelineCreateInfoCompl = pipelineCreateInfo;
	pipelineCreateInfoCompl.layout = layout;
	pipelineCreateInfoCompl.renderPass = myForwardPass;
	pipelineCreateInfoCompl.subpass = 0;
	pipelineCreateInfoCompl.pViewportState = &myViewportState;

	VkPipeline pipeline;
	auto resultPipeline = vkCreateGraphicsPipelines(myDevice,
													 VK_NULL_HANDLE,
													 1,
													 &pipelineCreateInfoCompl,
													 nullptr,
													 &pipeline);

	if (!resultPipeline)
	{
		auto& [setPipeline, setLayout] = myPipelines[myNumPipelines++];
		setPipeline = pipeline;
		setLayout = layout;
	}

	return {resultPipeline, pipeline, layout};
}

VkDevice
VulkanFramework::GetDevice()
{
	return myDevice;
}

VkPhysicalDevice
VulkanFramework::GetPhysicalDevice()
{
	return myPhysicalDevices[myChosenPhysicalDevice];
}

VkPhysicalDeviceMemoryProperties
VulkanFramework::GetPhysicalDeviceMemProps()
{
	return myPhysicalDeviceMemProperties;
}

void
VulkanFramework::BeginBackBufferRenderPass(
	VkCommandBuffer buffer, 
	uint32_t		framebufferIndex)
{
	VkRenderPassBeginInfo info;
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.pNext = nullptr;

	info.renderPass = myForwardPass;
	info.framebuffer = myFrameBuffers[framebufferIndex];
	info.renderArea = myScissor;
	info.clearValueCount = 2;
	VkClearValue clearColors[]{myClearColor, myDepthClearColor};
	info.pClearValues = clearColors;


	vkCmdBeginRenderPass(buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
}

int
VulkanFramework::AcquireNextSwapchainImage(VkSemaphore signalSemaphore)
{
	vkAcquireNextImageKHR(myDevice, mySwapchain, UINT64_MAX, signalSemaphore, nullptr, &myCurrentSwapchainIndex);
	return myCurrentSwapchainIndex;
}

void
VulkanFramework::Present(
	VkSemaphore waitSemaphore, 
	VkQueue		presentationQueue)
{
	VkPresentInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	info.pNext = nullptr;

	VkSemaphore waitSemaphores[]{waitSemaphore};
	info.waitSemaphoreCount = 1;
	info.pWaitSemaphores = waitSemaphores;

	info.swapchainCount = 1;
	info.pSwapchains = &mySwapchain;
	info.pImageIndices = &myCurrentSwapchainIndex;

	vkQueuePresentKHR(presentationQueue, &info);
}

std::tuple<float, float>
VulkanFramework::GetTargetResolution()
{
	return {abs(myViewport.width),abs(myViewport.height)};
}

VkRenderPass
VulkanFramework::GetForwardRenderPass()
{
	return myForwardPass;
}

VkAttachmentDescription
VulkanFramework::GetSwapchainAttachmentDesc() const
{
	return mySwapchainAttachmentDesc;
}

std::array<VkImageView, NumSwapchainImages>
VulkanFramework::GetSwapchainImageViews() const
{
	std::array<VkImageView, NumSwapchainImages> ret;
	memcpy(ret.data(), mySwapchainImageViews.data(), NumSwapchainImages * sizeof VkImageView);
	return ret;
}

neat::ThreadID VulkanFramework::GetMainThread() const
{
	return myMainThread;
}

VkResult
VulkanFramework::InitInstance()
{
	// APP INFO
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;

	appInfo.pApplicationName = "App";
	appInfo.applicationVersion = 100;
	appInfo.pEngineName = "Gamut";
	appInfo.engineVersion = 100;

	appInfo.apiVersion = VK_API_VERSION_1_3;

	// INSTANCE INFO

	// EXTENSIONS
	const char* extensionNames[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};

	VkInstanceCreateInfo instanceInfo{};
	// INSTANCE LAYERS

	if (gUseDebugLayers)
	{
		const char* layerNames[]
		{
			"VK_LAYER_KHRONOS_validation",
			//"VK_LAYER_LUNARG_standard_validation"
		};
		instanceInfo.enabledLayerCount = ARRAYSIZE(layerNames);
		instanceInfo.ppEnabledLayerNames = layerNames;
	}

	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = nullptr;

	instanceInfo.flags = NULL;
	instanceInfo.pApplicationInfo = &appInfo;


	instanceInfo.enabledExtensionCount = ARRAYSIZE(extensionNames);
	instanceInfo.ppEnabledExtensionNames = extensionNames;

	// CREATE INSTANCE
	auto resultInstance = vkCreateInstance(&instanceInfo, nullptr, &myInstance);
	VK_FALLTHROUGH(resultInstance);

	return VK_SUCCESS;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT		messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT				messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*										pUserData)
{
	bool printWorthy = false;
	std::string type;
	switch (messageSeverity)
	{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			type = "verbose";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			type = "info";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			printWorthy = true;
			type = "warning";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			printWorthy = true;
			type = "error";
			break;

	}

	if (printWorthy)
	{
		LOG('[', type, ']', pCallbackData->pMessage);
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		{
			system("pause");
		}
	}
	return VK_FALSE;
}

VkResult
VulkanFramework::InitDebugMessenger()
{
	if (gUseDebugLayers)
	{
		VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = DebugCallback;
		createInfo.pUserData = nullptr; // Optional

		auto createDebugMessenger = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(myInstance, "vkCreateDebugUtilsMessengerEXT");
		auto resultDebug = createDebugMessenger(myInstance, &createInfo, nullptr, &myDebugMessenger);
		VK_FALLTHROUGH(resultDebug);
	}

	return VK_SUCCESS;
}

VkResult
VulkanFramework::EnumeratePhysDevices()
{
	uint32_t gpuCount;
	auto resultEnumerate = vkEnumeratePhysicalDevices(myInstance, &gpuCount, nullptr);
	VK_FALLTHROUGH(resultEnumerate);
	myPhysicalDevices.resize(gpuCount);
	myPhysicalDeviceProperties.resize(gpuCount);
	resultEnumerate = vkEnumeratePhysicalDevices(myInstance, &gpuCount, myPhysicalDevices.data());
	VK_FALLTHROUGH(resultEnumerate);

	myChosenPhysicalDevice = UINT_MAX;
	for (uint32_t i = 0; i < gpuCount; i++)
	{
		VkPhysicalDeviceProperties pProps;
		vkGetPhysicalDeviceProperties(myPhysicalDevices[i], &pProps);
		myPhysicalDeviceProperties[i] = pProps;
		switch (pProps.deviceType)
		{
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:
				break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
				break;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
				myChosenPhysicalDevice = i;
				break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
				break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:
				break;
			default:;
		}
		if (myChosenPhysicalDevice != UINT_MAX)
		{
			break;
		}
	}
	if (myChosenPhysicalDevice == UINT_MAX)
	{
		LOG("no dedicated GPU found");
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}
	
	// FETCH PHYSICAL DEVICE MEM PROPS
	vkGetPhysicalDeviceMemoryProperties(myPhysicalDevices[myChosenPhysicalDevice], &myPhysicalDeviceMemProperties);

	// ENUMERATE EXTENSIONS
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	std::vector<VkExtensionProperties> globalProps;
	uint32_t count = 0;
	vkEnumerateDeviceExtensionProperties(myPhysicalDevices[myChosenPhysicalDevice], nullptr, &count, nullptr);
	globalProps.resize(count);
	vkEnumerateDeviceExtensionProperties(myPhysicalDevices[myChosenPhysicalDevice], nullptr, &count, globalProps.data());

	for (auto& layer : availableLayers)
	{
		std::vector<VkExtensionProperties> props;
		vkEnumerateDeviceExtensionProperties(myPhysicalDevices[myChosenPhysicalDevice], layer.layerName, &count, nullptr);
		props.resize(count);
		vkEnumerateDeviceExtensionProperties(myPhysicalDevices[myChosenPhysicalDevice], layer.layerName, &count, props.data());
		int val = 0;
	}

	return VK_SUCCESS;
}

VkResult
VulkanFramework::EnumerateQueueFamilies()
{
	// FILL IN QUEUE PROPS INFO
	uint32_t propCount;
	vkGetPhysicalDeviceQueueFamilyProperties(myPhysicalDevices[myChosenPhysicalDevice], &propCount, nullptr);
	myQueueFamilyProps.resize(propCount);
	vkGetPhysicalDeviceQueueFamilyProperties(myPhysicalDevices[myChosenPhysicalDevice], &propCount, myQueueFamilyProps.data());
	
	// ENUMERATE QUEUE FAMILIES
	QueueFamilyIndex index = 0;
	for (auto& queue : myQueueFamilyProps)
	{
		myQueueFlagsIndices[static_cast<VkQueueFlagBits>(queue.queueFlags)].emplace_back(index);
		if (queue.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			myQueueFlagsIndices[VK_QUEUE_GRAPHICS_BIT].emplace_back(index);
		}
		if (queue.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			myQueueFlagsIndices[VK_QUEUE_COMPUTE_BIT].emplace_back(index);
		}
		if (queue.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			myQueueFlagsIndices[VK_QUEUE_TRANSFER_BIT].emplace_back(index);
		}
		if (queue.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
		{
			myQueueFlagsIndices[VK_QUEUE_SPARSE_BINDING_BIT].emplace_back(index);
		}
		myQueuesLeft[index] = queue.queueCount;
		++index;
	}
	if (myQueueFlagsIndices.find(VK_QUEUE_GRAPHICS_BIT) == myQueueFlagsIndices.end())
	{
		return VK_ERROR_FEATURE_NOT_PRESENT;
	}

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitDevice()
{
	// QUEUE INFO
	float queuePriorities[32]{};
	std::vector<VkDeviceQueueCreateInfo> queueInfos;
	int queueIndex = 0;
	for (auto& queueFamily : myQueueFamilyProps)
	{
		//
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.pNext = nullptr;
		queueInfo.flags = NULL;

		queueInfo.queueFamilyIndex = queueIndex;
		queueInfo.queueCount = queueFamily.queueCount;
		queueInfo.pQueuePriorities = queuePriorities;

		queueInfos.emplace_back(queueInfo);
		++queueIndex;
	}



	// DEVICE EXTENSIONS
	const char* extensionNames[] =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_MAINTENANCE3_EXTENSION_NAME,
		VK_NV_RAY_TRACING_EXTENSION_NAME,
		VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
		//VK_EXT_FILTER_CUBIC_EXTENSION_NAME,
	};

	//vkGetDeviceProcAddr(myDevice, "vkCreateAccelerationStructureNV");



	// DEVICE INFO
	VkDeviceCreateInfo deviceInfo;
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext = nullptr;
	deviceInfo.flags = NULL;

	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
	deviceInfo.pQueueCreateInfos = queueInfos.data();
	deviceInfo.enabledExtensionCount = ARRAYSIZE(extensionNames);
	deviceInfo.ppEnabledExtensionNames = extensionNames;
	deviceInfo.enabledLayerCount = 0;
	deviceInfo.ppEnabledLayerNames = nullptr;

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(myPhysicalDevices[myChosenPhysicalDevice], &features);
	deviceInfo.pEnabledFeatures = &features;

	// CREATE DEVICE
	auto resultDevice = vkCreateDevice(myPhysicalDevices[myChosenPhysicalDevice], &deviceInfo, nullptr, &myDevice);
	VK_FALLTHROUGH(resultDevice);



	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitCmdPoolAndBuffer()
{
	int queueIndex = 0;
	for (auto& queueFamily : myQueueFamilyProps)
	{
		// POOL INFO
		VkCommandPoolCreateInfo poolInfo;
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.pNext = nullptr;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

		poolInfo.queueFamilyIndex = queueIndex;

		// CREATE POOL
		auto& [pool, data] = myCmdPoolsAndBuffers[queueIndex];

		auto resultPool = vkCreateCommandPool(myDevice, &poolInfo, nullptr, &pool);
		VK_FALLTHROUGH(resultPool)
			++queueIndex;
	}



	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitSurface(
	void* hWND)
{
	const HMODULE hModule = GetModuleHandle(nullptr);

	VkWin32SurfaceCreateInfoKHR surfaceInfo;
	surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.flags = NULL;

	surfaceInfo.hwnd = static_cast<HWND>(hWND);
	surfaceInfo.hinstance = hModule;


	auto resultSurface = vkCreateWin32SurfaceKHR(myInstance, &surfaceInfo, nullptr, &mySurface);
	VK_FALLTHROUGH(resultSurface);

	for (auto& index : myQueueFlagsIndices[VK_QUEUE_GRAPHICS_BIT])
	{
		VkBool32 supported;
		vkGetPhysicalDeviceSurfaceSupportKHR(myPhysicalDevices[myChosenPhysicalDevice], index, mySurface, &supported);
		if (!supported)
		{
			return VK_ERROR_INITIALIZATION_FAILED;
		}
	}

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitSwapchain(
	const Vec2ui& windowRes)
{
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &capabilities);

	std::vector<VkSurfaceFormatKHR> formats;
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &formatCount, nullptr);
	formats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &formatCount, formats.data());

	std::vector<VkPresentModeKHR> presentModes;
	uint32_t presentCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &presentCount, nullptr);
	presentModes.resize(presentCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &presentCount, presentModes.data());

	VkPresentModeKHR preferredPresentModes[] =
	{
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_FIFO_RELAXED_KHR,
	};
	VkPresentModeKHR chosenPresentMode = presentModes[0];
	for (int i = 0; i < ARRAYSIZE(preferredPresentModes); ++i)
	{
		if (std::find(presentModes.begin(), presentModes.end(), preferredPresentModes[i]) != presentModes.end())
		{
			chosenPresentMode = preferredPresentModes[i];
			break;
		}
	}

	// SWAP CHAIN INFO
	VkSwapchainCreateInfoKHR swapInfo;
	swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapInfo.pNext = nullptr;
	swapInfo.flags = NULL;

	swapInfo.surface = mySurface;
	swapInfo.minImageCount = NumSwapchainImages;
	swapInfo.imageFormat = formats[0].format;
	swapInfo.imageColorSpace = formats[0].colorSpace;
	swapInfo.imageExtent = { windowRes.x, windowRes.y};
	swapInfo.imageArrayLayers = 1;
	swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapInfo.queueFamilyIndexCount = 1;
	swapInfo.pQueueFamilyIndices = {0};

	swapInfo.preTransform = capabilities.currentTransform;;
	swapInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapInfo.presentMode = chosenPresentMode;
	swapInfo.clipped = VK_TRUE;
	swapInfo.oldSwapchain = nullptr;

	auto resultSwapchain = vkCreateSwapchainKHR(myDevice, &swapInfo, nullptr, &mySwapchain);
	VK_FALLTHROUGH(resultSwapchain);

	// SWAPCHAIN ATTACHMENT DESC
	mySwapchainAttachmentDesc = {};
	mySwapchainAttachmentDesc.format = formats[0].format;
	mySwapchainAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

	mySwapchainAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	mySwapchainAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	mySwapchainAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	mySwapchainAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	mySwapchainAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	mySwapchainAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitSwapchainImageViews()
{
	std::vector<VkSurfaceFormatKHR> formats;
	uint32_t count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &count, nullptr);
	formats.resize(count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &count, formats.data());

	uint32_t imageCount;
	std::vector<VkImage> swapChainImages;
	vkGetSwapchainImagesKHR(myDevice, mySwapchain, &imageCount, nullptr);
	swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(myDevice, mySwapchain, &imageCount, swapChainImages.data());

	for (auto& image : swapChainImages)
	{
		VkImageViewCreateInfo viewInfo;
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.pNext = nullptr;

		viewInfo.flags = NULL;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = formats[0].format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		mySwapchainImageViews.emplace_back();
		auto result = vkCreateImageView(myDevice, &viewInfo, nullptr, &mySwapchainImageViews.back());
		VK_FALLTHROUGH(result);
	}

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitRenderPasses()
{
	std::vector<VkSurfaceFormatKHR> formats;
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &formatCount, nullptr);
	formats.resize(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(myPhysicalDevices[myChosenPhysicalDevice], mySurface, &formatCount, formats.data());


	// ATTACHMENTS

	// COLOR
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = formats[0].format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// DEPTH
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = VK_FORMAT_D32_SFLOAT;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// SUBPASS
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	// SUBPASS DEPENDENCY
	VkSubpassDependency dependency{};
	dependency.dependencyFlags = NULL;
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = NULL;

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// RENDER PASS
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	VkAttachmentDescription attachments[]{colorAttachment, depthAttachment};
	renderPassInfo.pAttachments = attachments;

	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	VK_FALLTHROUGH(vkCreateRenderPass(myDevice, &renderPassInfo, nullptr, &myForwardPass));

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitViewport(
	const Vec2ui& windowRes)
{
	// VIEWPORT
	myViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	myViewportState.pNext = nullptr;
	myViewportState.flags = NULL;

	myScissor.offset = {0, 0};
	myScissor.extent = { windowRes.x, windowRes.y };

	myViewportState.scissorCount = 1;
	myViewportState.pScissors = &myScissor;

	myViewport.width = windowRes.x;
	myViewport.height = -static_cast<float>(windowRes.y);
	myViewport.x = 0;
	myViewport.y = windowRes.y;
	myViewport.minDepth = 0.f;
	myViewport.maxDepth = 1.f;

	myViewportState.viewportCount = 1;
	myViewportState.pViewports = &myViewport;

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitDepthImage(
	const Vec2ui& windowRes)
{
	// IMAGE
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.flags = NULL;

	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_D32_SFLOAT;
	imageInfo.extent.width = windowRes.x;
	imageInfo.extent.height = windowRes.y;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	QueueFamilyIndex index = myQueueFlagsIndices[VK_QUEUE_GRAPHICS_BIT].front();
	imageInfo.pQueueFamilyIndices = &index;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_FALLTHROUGH(vkCreateImage(myDevice, &imageInfo, nullptr, &myDepthImage));

	// MEM ALLOC
	VkMemoryRequirements memReq{};
	vkGetImageMemoryRequirements(myDevice, myDepthImage, &memReq);

	int chosenIndex = -1;
	for (int i = 0; i < myPhysicalDeviceMemProperties.memoryTypeCount; ++i)
	{
		if ((1 << i) & memReq.memoryTypeBits)
		{
			chosenIndex = i;
			break;
		}
	}

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = chosenIndex;

	VK_FALLTHROUGH(vkAllocateMemory(myDevice, &allocInfo, nullptr, &myDepthMemory));
	VK_FALLTHROUGH(vkBindImageMemory(myDevice, myDepthImage, myDepthMemory, 0));

	// VIEW
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.pNext = nullptr;
	viewInfo.flags = NULL;

	viewInfo.image = myDepthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = imageInfo.format;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	VK_FALLTHROUGH(vkCreateImageView(myDevice, &viewInfo, nullptr, &myDepthImageView));

	return VK_SUCCESS;
}

VkResult
VulkanFramework::InitFramebuffers(
	const Vec2ui& windowRes)
{

	for (auto& image : mySwapchainImageViews)
	{
		VkFramebufferCreateInfo fBufferInfo;
		fBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fBufferInfo.pNext = nullptr;
		fBufferInfo.flags = NULL;

		fBufferInfo.renderPass = myForwardPass;
		fBufferInfo.attachmentCount = 2;
		VkImageView attachments[]{image, myDepthImageView};
		fBufferInfo.pAttachments = attachments;
		fBufferInfo.width = windowRes.x;
		fBufferInfo.height = windowRes.y;
		fBufferInfo.layers = 1;

		VkFramebuffer fBuffer;
		auto result = vkCreateFramebuffer(myDevice, &fBufferInfo, nullptr, &fBuffer);
		VK_FALLTHROUGH(result);
		myFrameBuffers.emplace_back(fBuffer);
	}

	return VK_SUCCESS;
}
