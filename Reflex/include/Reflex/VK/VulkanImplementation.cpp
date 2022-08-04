#include "pch.h"
#include "VulkanImplementation.h"

#include "Memory/BufferAllocator.h"
#include "Uniform/UniformHandler.h"
#include "Mesh/MeshHandler.h"
#include "neat/Input/InputHandler.h"
#include "Memory/ImageAllocator.h"

#include "Image/ImageHandler.h"
#include "Memory/ImmediateTransferrer.h"
#include "Ray Tracing/AccelerationStructureAllocator.h"
#include "Ray Tracing/AccelerationStructureHandler.h"
#include "Scene/SceneGlobals.h"

#include "Debug/DebugUtils.h"
#include "Image/CubeFilterer.h"
#include "Presenter/Presenter.h"
#include "RenderPass/RenderPassFactory.h"
#include "Text/FontHandler.h"

#ifdef _DEBUG
#pragma comment (lib, "NEAT_Debugx64.lib")
#else
#pragma comment (lib, "NEAT_Releasex64.lib")
#endif

VulkanImplementation::VulkanImplementation()
{
}

VulkanImplementation::~VulkanImplementation()
{
	while (vkQueueWaitIdle(myPresentationQueue))
	{
	}
	while (vkQueueWaitIdle(myTransferQueue))
	{
	}

	for (uint32_t i = 0; i < NumSwapchainImages; ++i)
	{
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myImageAvailableSemaphore[i], nullptr);
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myFrameDoneSemaphore[i], nullptr);
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myHasTransferredSemaphore[i], nullptr);
	}

	for (auto& wSys : myWorkerSystems)
	{
		for (int scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
		{
			if (wSys.signalSemaphores[scIndex] != myFrameDoneSemaphore[scIndex])
			{
				vkDestroySemaphore(myVulkanFramework.GetDevice(), wSys.signalSemaphores[scIndex], nullptr);
			}
		}
	}

	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		vkDestroyFence(myVulkanFramework.GetDevice(), myTransferFences[swapchainIndex], nullptr);
	}
}

VkResult
VulkanImplementation::Initialize(
	const WindowInfo&	windowInfo,
	bool				useDebugLayers)
{
	VK_FALLTHROUGH(myVulkanFramework.Init(windowInfo, useDebugLayers));
	VK_FALLTHROUGH(InitSync());

	// PRESENTATION QUEUE
	auto [resultPresQueue, presQueue, presQueueFamily] = myVulkanFramework.RequestQueue(VK_QUEUE_GRAPHICS_BIT);
	VK_FALLTHROUGH(resultPresQueue);
	myPresentationQueue = presQueue;
	myQueueFamilyIndices[0] = presQueueFamily;
	myPresQueueIndex = presQueueFamily;

	DebugSetObjectName("Presentation Queue", myPresentationQueue, VK_OBJECT_TYPE_QUEUE, myVulkanFramework.GetDevice());

	// TRANSFER QUEUE
	auto [resultTransQueue, transQueue, transQueueFamily] = myVulkanFramework.RequestQueue(VK_QUEUE_TRANSFER_BIT);
	VK_FALLTHROUGH(resultTransQueue);
	myTransferQueue = transQueue;
	myQueueFamilyIndices[1] = transQueueFamily;
	myTransQueueIndex = transQueueFamily;

	DebugSetObjectName("Transfer Queue", myTransferQueue, VK_OBJECT_TYPE_QUEUE, myVulkanFramework.GetDevice());

	// COMPUTE QUEUE
	auto [resultCompQueue, compQueue, compQueueFamily] = myVulkanFramework.RequestQueue(VK_QUEUE_COMPUTE_BIT);
	VK_FALLTHROUGH(resultCompQueue);
	myComputeQueue = compQueue;
	myQueueFamilyIndices[2] = compQueueFamily;
	myCompQueueIndex = compQueueFamily;

	DebugSetObjectName("Compute Queue", myComputeQueue, VK_OBJECT_TYPE_QUEUE, myVulkanFramework.GetDevice());

	// CORE, MEMORY
	myImmediateTransferrer = std::make_unique<ImmediateTransferrer>(myVulkanFramework);

	myAllocationSubmitter = std::make_unique<AllocationSubmitter>(myVulkanFramework, myTransQueueIndex);
	myBufferAllocator = std::make_unique<BufferAllocator>(myVulkanFramework, *myAllocationSubmitter, *myImmediateTransferrer, transQueueFamily);
	myImageAllocator = std::make_unique<ImageAllocator>(myVulkanFramework, *myAllocationSubmitter, *myImmediateTransferrer, transQueueFamily);
	myAccelerationStructureAllocator = std::make_unique<AccelerationStructureAllocator>(myVulkanFramework,
																		   *myAllocationSubmitter,
																		   *myBufferAllocator,
																		   *myImmediateTransferrer,
																		   transQueueFamily,
																		   presQueueFamily);

	myRenderPassFactory = std::make_unique<RenderPassFactory>(myVulkanFramework,
												 *myImageAllocator,
												 myQueueFamilyIndices.data(),
												 myQueueFamilyIndices.size());

	// PRIMARY TRANSFER BUFFERS
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		auto [result, cmdBuffer] = myVulkanFramework.RequestCommandBuffer(myTransQueueIndex);
		assert(!result && "failed requesting command buffer for primary transfer buffer");
		myTransferCmdBuffer[swapchainIndex] = cmdBuffer;

		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		result = vkCreateFence(myVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myTransferFences[swapchainIndex]);
	}

	// HANDLERS
	myUniformHandler = std::make_shared<UniformHandler>(myVulkanFramework,
										   *myBufferAllocator,
										   myQueueFamilyIndices.data(),
										   myQueueFamilyIndices.size());
	myUniformHandler->Init();

	myImageHandler = std::make_shared<ImageHandler>(myVulkanFramework,
									   *myImageAllocator,
									   myQueueFamilyIndices.data(),
									   myQueueFamilyIndices.size());
	myFontHandler = std::make_shared<FontHandler>(myVulkanFramework,
									*myImageHandler,
									myQueueFamilyIndices.data(),
									myQueueFamilyIndices.size());


	myMeshHandler = std::make_shared<MeshHandler>(myVulkanFramework,
									 *myBufferAllocator,
									 *myImageHandler,
									 myQueueFamilyIndices.data(),
									 myQueueFamilyIndices.size());

	myAccStructHandler = std::make_shared<AccelerationStructureHandler>(myVulkanFramework, *myAccelerationStructureAllocator);

	// ETC
	mySceneGlobals = std::make_shared<SceneGlobals>(myVulkanFramework,
									*myUniformHandler,
									   transQueueFamily,
									   presQueueFamily,
									   compQueueFamily);

	// WORKER SYSTEMS
	myPresenter = std::make_shared<Presenter>(myVulkanFramework,
								 *myRenderPassFactory,
								 *myImageHandler,
								 *mySceneGlobals,
								 myPresQueueIndex,
								 myTransQueueIndex);

	myCubeFilterer = std::make_shared<CubeFilterer>(myVulkanFramework,
										  *myRenderPassFactory,
										  *myImageAllocator,
										  *myImageHandler,
										  myPresQueueIndex,
										  myTransQueueIndex

	);
	myCubeFilterer->PushFilterWork(
		CubeID(0),
		CubeDimension::Dim64);

	RegisterWorkerSystem(myCubeFilterer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);

	LOG("vulkan successfully started");
	return VK_SUCCESS;
}

VkResult
VulkanImplementation::InitSync()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = NULL;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	VkResult resultSemaphore;
	for (uint32_t i = 0; i < NumSwapchainImages; ++i)
	{
		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myImageAvailableSemaphore[i]);
		DebugSetObjectName(std::string("Image Available Sem " + std::to_string(i)).c_str(), myImageAvailableSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);

		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myFrameDoneSemaphore[i]);
		DebugSetObjectName(std::string("Has Rendered Sem " + std::to_string(i)).c_str(), myFrameDoneSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);

		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myHasTransferredSemaphore[i]);
		DebugSetObjectName(std::string("Has Transferred Sem " + std::to_string(i)).c_str(), myHasTransferredSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);
	}

	return VK_SUCCESS;
}

void
VulkanImplementation::SubmitTransferCmds()
{
	while (vkGetFenceStatus(myVulkanFramework.GetDevice(), myTransferFences[mySwapchainImageIndex]))
	{
	}
	vkResetFences(myVulkanFramework.GetDevice(), 1, &myTransferFences[mySwapchainImageIndex]);
	
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(myTransferCmdBuffer[mySwapchainImageIndex], &beginInfo);

	std::optional<AllocationSubmission> allocSub;
	int count = 0;
	constexpr int cMaxAllocPerFrame = 128;
	while ((allocSub = myAllocationSubmitter->AcquireNextAllocSubmission(), allocSub.has_value())
		&& count++ < cMaxAllocPerFrame)
	{
		auto [cmdBuffer, event] = allocSub->Submit(myTransferFences[mySwapchainImageIndex]);
		vkCmdExecuteCommands(myTransferCmdBuffer[mySwapchainImageIndex], 1, &cmdBuffer);
		vkCmdSetEvent(myTransferCmdBuffer[mySwapchainImageIndex], event, VK_PIPELINE_STAGE_TRANSFER_BIT);
		myAllocationSubmitter->QueueRelease(std::move(allocSub.value()));
	}
	
	vkEndCommandBuffer(myTransferCmdBuffer[mySwapchainImageIndex]);

	VkSubmitInfo transferSubmitInfo{};
	transferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transferSubmitInfo.pNext = nullptr;

	transferSubmitInfo.commandBufferCount = 1;
	transferSubmitInfo.pCommandBuffers = &myTransferCmdBuffer[mySwapchainImageIndex];
	transferSubmitInfo.signalSemaphoreCount = 1;
	transferSubmitInfo.pSignalSemaphores = &myHasTransferredSemaphore[mySwapchainImageIndex];

	auto result = vkQueueSubmit(myTransferQueue, 1, &transferSubmitInfo, myTransferFences[mySwapchainImageIndex]);
	assert(!result && "failed submission");
}

void
VulkanImplementation::SubmitWorkerCmds()
{
	//for (auto& wSys : myWorkerSystems)
	//{
	//	auto [submitInfo, fence] = wSys.system->RecordSubmit(mySwapchainImageIndex,
	//														  wSys.waitSemaphores[mySwapchainImageIndex].semaphores.data(),
	//														  wSys.numWaitSemaphores,
	//														  wSys.waitSemaphores[mySwapchainImageIndex].stages.data(),
	//														  wSys.numWaitSemaphores,
	//														  &wSys.signalSemaphores[mySwapchainImageIndex]
	//	);
	//	vkResetFences(myVulkanFramework.GetDevice(), 1, &fence);
	//	auto resultSubmit = vkQueueSubmit(wSys.subQueue, 1, &submitInfo, fence);
	//	assert(!resultSubmit && "failed submission");
	//}

	for (auto iter = myWorkerSystems.begin(); iter != myWorkerSystems.end() - 1; ++iter)
	{
		auto& wSys = *iter;
		auto [submitInfo, fence] = wSys.system->RecordSubmit(mySwapchainImageIndex,
			wSys.waitSemaphores[mySwapchainImageIndex].semaphores.data(),
			wSys.numWaitSemaphores,
			wSys.waitSemaphores[mySwapchainImageIndex].stages.data(),
			wSys.numWaitSemaphores,
			&wSys.signalSemaphores[mySwapchainImageIndex]
		);
		vkResetFences(myVulkanFramework.GetDevice(), 1, &fence);
		auto resultSubmit = vkQueueSubmit(wSys.subQueue, 1, &submitInfo, fence);
		assert(!resultSubmit && "failed submission");
	}
	{
		auto& wSys = myWorkerSystems.back();
		auto [submitInfo, fence] = wSys.system->RecordSubmit(mySwapchainImageIndex,
			wSys.waitSemaphores[mySwapchainImageIndex].semaphores.data(),
			wSys.numWaitSemaphores,
			wSys.waitSemaphores[mySwapchainImageIndex].stages.data(),
			wSys.numWaitSemaphores,
			&wSys.signalSemaphores[mySwapchainImageIndex]
		);
		
		vkResetFences(myVulkanFramework.GetDevice(), 1, &fence);
		auto resultSubmit = vkQueueSubmit(wSys.subQueue, 1, &submitInfo, fence);
		assert(!resultSubmit && "failed submission");
	}
}

void
VulkanImplementation::BeginFrame()
{
	assert(myWorkerSystemsLocked && "worker systems not locked");
	static int fnr = -1;
	mySwapchainImageIndex = myVulkanFramework.AcquireNextSwapchainImage(myImageAvailableSemaphore[++fnr % NumSwapchainImages]);

	const int swapchainIndexToUpdate = (fnr + 1) % NumSwapchainImages;
	myImageHandler->UpdateDescriptors(swapchainIndexToUpdate, myWorkerSystemsFences[swapchainIndexToUpdate]);
	myMeshHandler->UpdateDescriptors(swapchainIndexToUpdate, myWorkerSystemsFences[swapchainIndexToUpdate]);
	myAllocationSubmitter->TryReleasing();
}

void
VulkanImplementation::Submit()
{
	SubmitTransferCmds();
	SubmitWorkerCmds();
}

void
VulkanImplementation::EndFrame()
{
	myVulkanFramework.Present(myFrameDoneSemaphore[mySwapchainImageIndex], myPresentationQueue);
}

void
VulkanImplementation::RegisterWorkerSystem(
	std::shared_ptr<WorkerSystem>	system,
	VkPipelineStageFlags			waitStage,
	VkQueueFlagBits					subQueueType)
{
	SlottedWorkerSystem toSlot{};
	toSlot.system = system;

	switch (subQueueType)
	{
		case VK_QUEUE_GRAPHICS_BIT:
			toSlot.subQueue = myPresentationQueue;
			break;
		case VK_QUEUE_COMPUTE_BIT:
			toSlot.subQueue = myComputeQueue;
			break;
		case VK_QUEUE_TRANSFER_BIT:
			toSlot.subQueue = myTransferQueue;
			break;
		default:
			assert("invalid submission queue type" && false);
			break;
	}

	// SIGNAL SEMAPHORES
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = NULL;

	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		auto resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &toSlot.signalSemaphores[scIndex]);
		DebugSetObjectName(std::string("WorkerSystem no.  " + std::to_string(scIndex)).c_str(),
							toSlot.signalSemaphores[scIndex],
							VK_OBJECT_TYPE_SEMAPHORE,
							myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);
	}

	// WAIT SEMAPHORES
	if (myWorkerSystems.empty())
	{
		toSlot.AddWaitSemaphore(myHasTransferredSemaphore, waitStage);
	}
	else
	{
		SlottedWorkerSystem& pre = myWorkerSystems.back();
		toSlot.AddWaitSemaphore(pre.signalSemaphores, waitStage);
	}

	myWorkerSystems.emplace_back(toSlot);
}

void
VulkanImplementation::LockWorkerSystems()
{
	RegisterWorkerSystem(myPresenter, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);
	myWorkerSystems.back().AddWaitSemaphore(myImageAvailableSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

	for (int scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myWorkerSystems.back().signalSemaphores[scIndex], nullptr);
	}
	myWorkerSystems.back().signalSemaphores = myFrameDoneSemaphore;
	myWorkerSystemsLocked = true;

	myWorkerSystemsFences = myWorkerSystems.back().system->GetFences();
}

void VulkanImplementation::RegisterThread(
	neat::ThreadID threadID)
{
	if (BAD_ID(threadID))
	{
		LOG("bad thread ID");
		return;
	}
	for (auto& workerSystem : myWorkerSystems)
	{
		workerSystem.system->AddSchedule(threadID);
	}
	myAllocationSubmitter->RegisterThread(threadID);
}

