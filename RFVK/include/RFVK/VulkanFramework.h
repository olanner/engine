
#pragma once

class VulkanFramework
{
public:
										VulkanFramework();
										~VulkanFramework();

	VkResult							Init(
											neat::ThreadID	threadID, 
											void*			hWND,
											const Vec2ui&	windowRes, 
											bool			useDebugLayers = false);
	std::tuple<VkResult, VkQueue, QueueFamilyIndex>
										RequestQueue(VkQueueFlagBits queueType);
	std::tuple<VkResult, VkCommandBuffer>
										RequestCommandBuffer(QueueFamilyIndex index);

	std::tuple<VkResult, VkPipeline, VkPipelineLayout>
										CreatePipeline(
											const VkGraphicsPipelineCreateInfo& pipelineCreateInfo,
											const VkPipelineLayoutCreateInfo&	layoutCreateInfo);

	VkDevice							GetDevice();
	VkPhysicalDevice					GetPhysicalDevice();
	VkPhysicalDeviceMemoryProperties	GetPhysicalDeviceMemProps();
	void								BeginBackBufferRenderPass(
											VkCommandBuffer buffer, 
											uint32_t framebufferIndex);

	int									AcquireNextSwapchainImage(VkSemaphore signalSemaphore);
	void								Present(
											VkSemaphore waitSemaphore, 
											VkQueue		presentationQueue);


	auto								GetFramebufferImageViews() const
	{
		return mySwapchainImageViews;
	}

	std::tuple<float, float>			GetTargetResolution();

	VkRenderPass						GetForwardRenderPass();

	VkAttachmentDescription				GetSwapchainAttachmentDesc() const;
	std::array<VkImageView, NumSwapchainImages>
										GetSwapchainImageViews() const;
	neat::ThreadID						GetMainThread() const;

private:
	VkResult							InitInstance();
	VkResult							InitDebugMessenger();
	VkResult							EnumeratePhysDevices();
	VkResult							EnumerateQueueFamilies();
	VkResult							InitDevice();
	VkResult							InitCmdPoolAndBuffer();
	VkResult							InitSurface(void* hWND);
	VkResult							InitSwapchain(const Vec2ui& windowRes);
	VkResult							InitSwapchainImageViews();

	VkResult							InitRenderPasses();
	VkResult							InitViewport(const Vec2ui& windowRes);
	VkResult							InitDepthImage(const Vec2ui& windowRes);
	VkResult							InitFramebuffers(const Vec2ui& windowRes);

	neat::ThreadID						myMainThread;
	std::vector<VkPhysicalDevice>		myPhysicalDevices;
	std::vector<VkPhysicalDeviceProperties>
										myPhysicalDeviceProperties;
	
	uint32_t							myChosenPhysicalDevice = 0;
	std::vector<VkQueueFamilyProperties>
										myQueueFamilyProps;
	std::unordered_map<VkQueueFlagBits/*Queue Flag*/, std::vector<QueueFamilyIndex>/*Queue Family Index*/>
										myQueueFlagsIndices;

	VkInstance							myInstance = nullptr;
	VkDebugUtilsMessengerEXT			myDebugMessenger = nullptr;
	VkDevice							myDevice = nullptr;

	VkPhysicalDeviceMemoryProperties	myPhysicalDeviceMemProperties = {};

	VkSurfaceKHR						mySurface = nullptr;
	VkSwapchainKHR						mySwapchain = nullptr;

	VkAttachmentDescription				mySwapchainAttachmentDesc = {};
	neat::static_vector<VkImageView, NumSwapchainImages>
										mySwapchainImageViews;
	neat::static_vector<VkFramebuffer, NumSwapchainImages>
										myFrameBuffers;
	uint32_t							myCurrentSwapchainIndex = 0;
	VkImage								myDepthImage = nullptr;
	VkImageView							myDepthImageView = nullptr;
	VkDeviceMemory						myDepthMemory = nullptr;

	std::unordered_map<QueueFamilyIndex, int/*queues left*/>
										myQueuesLeft;
	std::unordered_map<QueueFamilyIndex/*Queue Family Index*/, std::tuple<VkCommandPool, std::vector<VkCommandBuffer>>>
										myCmdPoolsAndBuffers;

	VkRect2D							myScissor = {};
	VkViewport							myViewport = {};
	VkPipelineViewportStateCreateInfo	myViewportState = {};
	VkRenderPass						myForwardPass = nullptr;
	std::array<std::pair<VkPipeline, VkPipelineLayout>, 32>
										myPipelines;
	uint32_t							myNumPipelines = 0;

	VkClearValue						myClearColor = {};
	VkClearValue						myDepthClearColor = {};

};