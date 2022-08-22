
#pragma once

#include "VulkanFramework.h"
#include "WorkerSystem/WorkerSystem.h"
#include "neat/General/Thread.h"

namespace rflx
{
	class Reflex;
}

class VulkanImplementation
{
	friend class rflx::Reflex;
public:
												VulkanImplementation();
												~VulkanImplementation();

	VkResult									Initialize(
													neat::ThreadID		threadID,
													const WindowInfo&	windowInfo, 
													bool				useDebugLayers = false);

	void										BeginFrame();
	void										Submit();
	void										EndFrame();

	void										RegisterWorkerSystem(
													std::shared_ptr<WorkerSystem>	system,
													VkPipelineStageFlags			waitStage,
													VkQueueFlagBits					subQueueType);
	void										LockWorkerSystems();
	void										RegisterThread(neat::ThreadID threadID);

private:
	VkResult									InitSync();

	void										SubmitTransferCmds();
	void										SubmitWorkerCmds();

	VulkanFramework								myVulkanFramework;
	VkQueue										myPresentationQueue = nullptr;
	VkQueue										myTransferQueue = nullptr;
	VkQueue										myComputeQueue = nullptr;
	std::array<QueueFamilyIndex, 3>				myQueueFamilyIndices = {};
	QueueFamilyIndex							myPresQueueIndex = 0;
	QueueFamilyIndex							myTransQueueIndex = 0;
	QueueFamilyIndex							myCompQueueIndex = 0;
	
	std::array<VkCommandBuffer, NumSwapchainImages>
												myTransferCmdBuffer = {};
	std::array<VkFence, NumSwapchainImages>		myTransferFences = {};

	std::array<VkSemaphore, NumSwapchainImages> myHasTransferredSemaphore = {};

	std::array<VkSemaphore, NumSwapchainImages> myImageAvailableSemaphore = {};
	std::array<VkSemaphore, NumSwapchainImages> myFrameDoneSemaphore = {};

	uint8_t										mySwapchainImageIndex = 0;

	// CORE, MEMORY
	std::unique_ptr<class ImmediateTransferrer> myImmediateTransferrer = nullptr;

	std::unique_ptr<class AllocationSubmitter>  myAllocationSubmitter = nullptr;
	std::unique_ptr<class BufferAllocator>		myBufferAllocator = nullptr;
	std::unique_ptr<class ImageAllocator>		myImageAllocator = nullptr;
	std::unique_ptr<class AccelerationStructureAllocator>
												myAccelerationStructureAllocator = nullptr;

	std::unique_ptr<class RenderPassFactory>	myRenderPassFactory = nullptr;

	// OBJECT HANDLERS
	std::shared_ptr<class UniformHandler>		myUniformHandler = nullptr;
	std::shared_ptr<class ImageHandler>			myImageHandler = nullptr;
	std::shared_ptr<class FontHandler>			myFontHandler = nullptr;
	std::shared_ptr<class MeshHandler>			myMeshHandler = nullptr;
	std::shared_ptr<class AccelerationStructureHandler>
												myAccStructHandler = nullptr;

	// ETC
	std::shared_ptr<class SceneGlobals>			mySceneGlobals = nullptr;

	// WORKERS
	std::vector<SlottedWorkerSystem>
												myWorkerSystems;
	std::array<VkFence, NumSwapchainImages>		myWorkerSystemsFences = {};
	std::shared_ptr<class CubeFilterer>			myCubeFilterer = nullptr;
	std::shared_ptr<class Presenter>			myPresenter = nullptr;
	bool										myWorkerSystemsLocked = false;

	bool										myUseRayTracing = false;

};
