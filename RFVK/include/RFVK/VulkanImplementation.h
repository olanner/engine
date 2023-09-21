
#pragma once

#include "VulkanFramework.h"
#include "WorkerSystem/WorkerSystem.h"
#include "neat/General/Thread.h"
#include "Features.h"

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
													void*				hWND, 
													const Vec2ui&		windowRes, 
													bool				useDebugLayers = false);

	void										BeginFrame();
	void										Submit();
	void										EndFrame();

	void										RegisterWorkerSystem(std::shared_ptr<WorkerSystem> system);
	void										LockWorkerSystems();
	void										RegisterThread(neat::ThreadID threadID);
	bool										CheckFeature(rflx::Features feature);
	void										ToggleFeature(rflx::Features feature);

private:
	VkResult									InitSync();

	void										SubmitTransferCmds();
	void										SubmitWorkerCmds();

	VulkanFramework								myVulkanFramework;
	VkQueue										myGraphicsQueue = nullptr;
	VkQueue										myTransferQueue = nullptr;
	VkQueue										myComputeQueue = nullptr;
	QueueFamilyIndices							myQueueFamilyIndices = {};
	//QueueFamilyIndex							myPresQueueIndex = 0;
	//QueueFamilyIndex							myTransQueueIndex = 0;
	//QueueFamilyIndex							myCompQueueIndex = 0;
	
	std::array<VkCommandBuffer, NumSwapchainImages>
												myTransferCmdBuffer = {};
	std::array<VkFence, NumSwapchainImages>		myTransferFences = {};

	std::array<VkSemaphore, NumSwapchainImages> myHasTransferredSemaphore = {};

	std::array<VkSemaphore, NumSwapchainImages> myImageAvailableSemaphore = {};
	std::array<VkSemaphore, NumSwapchainImages> myFrameDoneSemaphore = {};

	uint8_t										mySwapchainImageIndex = 0;

	// CORE, MEMORY
	std::unique_ptr<class ImmediateTransferrer> myImmediateTransferrer;

	std::unique_ptr<class AllocationSubmitter>  myAllocationSubmitter;
	std::unique_ptr<class BufferAllocator>		myBufferAllocator;
	std::unique_ptr<class ImageAllocator>		myImageAllocator;
	std::unique_ptr<class AccelerationStructureAllocator>
												myAccelerationStructureAllocator;

	std::unique_ptr<class RenderPassFactory>	myRenderPassFactory;

	// OBJECT HANDLERS
	std::shared_ptr<class UniformHandler>		myUniformHandler;
	std::shared_ptr<class ImageHandler>			myImageHandler;
	std::shared_ptr<class FontHandler>			myFontHandler;
	std::shared_ptr<class MeshHandler>			myMeshHandler;
	std::shared_ptr<class AccelerationStructureHandler>
												myAccStructHandler;

	// ETC
	std::shared_ptr<class SceneGlobals>			mySceneGlobals;

	// WORKERS
	std::vector<SlottedWorkerSystem>
												myWorkerSystems;
	std::array<VkFence, NumSwapchainImages>		myWorkerSystemsFences = {};
	std::vector<int>							myWorkersOrder;
	
	std::shared_ptr<class CubeFilterer>			myCubeFilterer;
	std::shared_ptr<class Presenter>			myPresenter;
	bool										myWorkerSystemsLocked = false;

	conc_map<rflx::Features, bool>				myActiveFeatures;

};
