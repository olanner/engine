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
	vkDeviceWaitIdle(myVulkanFramework.GetDevice());
	
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
			for (int subIndex = 0; subIndex < wSys.system->GetSubmissionCount(); ++subIndex)
			{
				if (wSys.signalSemaphores[scIndex][subIndex] != myFrameDoneSemaphore[scIndex])
				{
					vkDestroySemaphore(myVulkanFramework.GetDevice(), wSys.signalSemaphores[scIndex][subIndex], nullptr);
				}
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
	neat::ThreadID		threadID,
	void*				hWND,
	const Vec2ui&		windowRes,
	bool				useDebugLayers)
{
	VK_FALLTHROUGH(myVulkanFramework.Init(threadID, hWND, windowRes, useDebugLayers));
	VK_FALLTHROUGH(InitSync());

	// PRESENTATION QUEUE
	auto [resultPresQueue, presQueue, presQueueFamily] = myVulkanFramework.RequestQueue(VK_QUEUE_GRAPHICS_BIT);
	VK_FALLTHROUGH(resultPresQueue);
	myGraphicsQueue = presQueue;
	myQueueFamilyIndices[QUEUE_FAMILY_GRAPHICS] = presQueueFamily;

	DebugSetObjectName("Presentation Queue", myGraphicsQueue, VK_OBJECT_TYPE_QUEUE, myVulkanFramework.GetDevice());

	// TRANSFER QUEUE
	auto [resultTransQueue, transQueue, transQueueFamily] = myVulkanFramework.RequestQueue(VK_QUEUE_TRANSFER_BIT);
	VK_FALLTHROUGH(resultTransQueue);
	myTransferQueue = transQueue;
	myQueueFamilyIndices[QUEUE_FAMILY_TRANSFER] = transQueueFamily;

	DebugSetObjectName("Transfer Queue", myTransferQueue, VK_OBJECT_TYPE_QUEUE, myVulkanFramework.GetDevice());

	// COMPUTE QUEUE
	auto [resultCompQueue, compQueue, compQueueFamily] = myVulkanFramework.RequestQueue(VK_QUEUE_COMPUTE_BIT);
	VK_FALLTHROUGH(resultCompQueue);
	myComputeQueue = compQueue;
	myQueueFamilyIndices[QUEUE_FAMILY_COMPUTE] = compQueueFamily;

	DebugSetObjectName("Compute Queue", myComputeQueue, VK_OBJECT_TYPE_QUEUE, myVulkanFramework.GetDevice());

	// CORE, MEMORY
	myImmediateTransferrer = std::make_unique<ImmediateTransferrer>(myVulkanFramework);

	myAllocationSubmitter = std::make_unique<AllocationSubmitter>(myVulkanFramework, myQueueFamilyIndices[QUEUE_FAMILY_TRANSFER]);
	myAllocationSubmitter->RegisterThread(myVulkanFramework.GetMainThread());
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
		auto [result, cmdBuffer] = myVulkanFramework.RequestCommandBuffer(myQueueFamilyIndices[QUEUE_FAMILY_TRANSFER]);
		assert(!result && "failed requesting command buffer for primary transfer buffer");
		myTransferCmdBuffer[swapchainIndex] = cmdBuffer;
		DebugSetObjectName(
			std::string("transfer cmd buffer : ").append(std::to_string(swapchainIndex)).c_str(),
			cmdBuffer,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			myVulkanFramework.GetDevice());
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		result = vkCreateFence(myVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myTransferFences[swapchainIndex]);
	}

	// HANDLERS
	myUniformHandler = std::make_shared<UniformHandler>(myVulkanFramework,
										   *myBufferAllocator,
										   myQueueFamilyIndices);
	myUniformHandler->Init();

	myImageHandler = std::make_shared<ImageHandler>(myVulkanFramework,
									   *myImageAllocator,
									   myQueueFamilyIndices);
	myFontHandler = std::make_shared<FontHandler>(myVulkanFramework,
									*myImageHandler,
									myQueueFamilyIndices.data(),
									myQueueFamilyIndices.size());


	myMeshHandler = std::make_shared<MeshHandler>(myVulkanFramework,
									 *myBufferAllocator,
									 *myImageHandler,
		myQueueFamilyIndices);

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
								 myQueueFamilyIndices);

	myCubeFilterer = std::make_shared<CubeFilterer>(myVulkanFramework,
										  *myRenderPassFactory,
										  *myImageAllocator,
										  *myImageHandler,
										  myQueueFamilyIndices

	);
	myCubeFilterer->PushFilterWork(
		{CubeID(0),
		CubeDimension::Dim64,
		nullptr});

	RegisterWorkerSystem(myCubeFilterer);

	myActiveFeatures[rflx::Features::FEATURE_CORE] = true;
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
	
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(myTransferCmdBuffer[mySwapchainImageIndex], &beginInfo);

	std::optional<AllocationSubmissionID> allocSubID;
	int count = 0;
	constexpr int cMaxAllocPerFrame = 128;
	VkCommandBuffer prevBuffer = nullptr;
	while ((allocSubID = myAllocationSubmitter->AcquireNextAllocSubmission()).has_value()
		&& count++ < cMaxAllocPerFrame)
	{
		auto& allocSub = (*myAllocationSubmitter)[allocSubID.value()];
		auto [cmdBuffer, event] = allocSub.Submit(myWorkerSystemsFences[mySwapchainImageIndex]);
		vkCmdExecuteCommands(myTransferCmdBuffer[mySwapchainImageIndex], 1, &cmdBuffer);
		vkCmdSetEvent(myTransferCmdBuffer[mySwapchainImageIndex], event, VK_PIPELINE_STAGE_TRANSFER_BIT);
		myAllocationSubmitter->QueueRelease(std::move(allocSubID.value()));
		prevBuffer = cmdBuffer;
	}
	
	vkEndCommandBuffer(myTransferCmdBuffer[mySwapchainImageIndex]);

	VkSubmitInfo transferSubmitInfo{};
	transferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transferSubmitInfo.pNext = nullptr;

	transferSubmitInfo.commandBufferCount = 1;
	transferSubmitInfo.pCommandBuffers = &myTransferCmdBuffer[mySwapchainImageIndex];
	transferSubmitInfo.signalSemaphoreCount = 1;
	transferSubmitInfo.pSignalSemaphores = &myHasTransferredSemaphore[mySwapchainImageIndex];

	vkResetFences(myVulkanFramework.GetDevice(), 1, &myTransferFences[mySwapchainImageIndex]);
	auto result = vkQueueSubmit(myTransferQueue, 1, &transferSubmitInfo, myTransferFences[mySwapchainImageIndex]);
	assert(!result && "failed submission");
}

void
VulkanImplementation::SubmitWorkerCmds()
{
	neat::static_vector<VkSemaphore, MaxWorkerSubmissions> waitSemaphores = {myHasTransferredSemaphore[mySwapchainImageIndex]};
	for (auto iter = myWorkersOrder.begin(); iter != myWorkersOrder.end() - 1; ++iter)
	{
		int index = *iter;
		auto& wSys = myWorkerSystems[index];
		auto submissions = wSys.system->RecordSubmit(
			mySwapchainImageIndex,
			waitSemaphores,
			wSys.signalSemaphores[mySwapchainImageIndex]);
		VkQueue queue = nullptr;
		for (auto& submission : submissions)
		{
			switch (submission.desiredQueue)
			{
				case VK_QUEUE_GRAPHICS_BIT: queue = myGraphicsQueue; break;
				case VK_QUEUE_COMPUTE_BIT: queue = myComputeQueue; break;
				case VK_QUEUE_TRANSFER_BIT: queue = myTransferQueue; break;
			}
			vkResetFences(myVulkanFramework.GetDevice(), 1, &submission.fence);
			const auto resultSubmit = vkQueueSubmit(queue, 1, &submission.submitInfo, submission.fence);
			assert(!resultSubmit && "failed submission");
		}
		waitSemaphores = wSys.signalSemaphores[mySwapchainImageIndex];
	}
	{
		auto& wSys = myWorkerSystems.back();
		waitSemaphores.emplace_back(myImageAvailableSemaphore[mySwapchainImageIndex]);
		auto submissions = wSys.system->RecordSubmit(
			mySwapchainImageIndex,
			waitSemaphores,
			wSys.signalSemaphores[mySwapchainImageIndex]);

		VkQueue queue = nullptr;
		for (auto& submission : submissions)
		{
			switch (submission.desiredQueue)
			{
				case VK_QUEUE_GRAPHICS_BIT: queue = myGraphicsQueue; break;
				case VK_QUEUE_COMPUTE_BIT: queue = myComputeQueue; break;
				case VK_QUEUE_TRANSFER_BIT: queue = myTransferQueue; break;
			}
			vkResetFences(myVulkanFramework.GetDevice(), 1, &submission.fence);
			const auto resultSubmit = vkQueueSubmit(queue, 1, &submission.submitInfo, submission.fence);
			assert(!resultSubmit && "failed submission");
		}
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
	myImageAllocator->DoCleanUp(128);
	myBufferAllocator->DoCleanUp(128);
	myAccelerationStructureAllocator->DoCleanUp(128);
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
	myVulkanFramework.Present(myFrameDoneSemaphore[mySwapchainImageIndex], myGraphicsQueue);
	myAllocationSubmitter->TryReleasing();
}

void
VulkanImplementation::RegisterWorkerSystem(
	std::shared_ptr<WorkerSystem>	system)
{
	SlottedWorkerSystem toSlot{};
	toSlot.system = system;
	// SIGNAL SEMAPHORES
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = NULL;

	int subCount = system->GetSubmissionCount();
	
	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		toSlot.signalSemaphores[scIndex].resize(subCount);
		for (int submissionIndex = 0; submissionIndex < subCount; ++submissionIndex)
		{
			auto resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &toSlot.signalSemaphores[scIndex][submissionIndex]);

			DebugSetObjectName(std::string("WorkerSystem no.  " + std::to_string(scIndex)).c_str(),
				toSlot.signalSemaphores[scIndex][submissionIndex],
				VK_OBJECT_TYPE_SEMAPHORE,
				myVulkanFramework.GetDevice());
			VK_FALLTHROUGH(resultSemaphore);
		}
	}

	// WAIT SEMAPHORES
	//if (myWorkerSystems.empty())
	//{
	//	toSlot.AddWaitSemaphore(myHasTransferredSemaphore, waitStage);
	//}
	//else
	//{
	//	SlottedWorkerSystem& pre = myWorkerSystems.back();
	//	toSlot.AddWaitSemaphore(pre.signalSemaphores, waitStage);
	//}

	myWorkerSystems.emplace_back(toSlot);
}

void
VulkanImplementation::LockWorkerSystems()
{
	RegisterWorkerSystem(myPresenter);

	for (int scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		int subCount = myWorkerSystems.back().system->GetSubmissionCount();
		for (int subIndex = 0; subIndex < subCount; ++subIndex)
		{
			vkDestroySemaphore(myVulkanFramework.GetDevice(), myWorkerSystems.back().signalSemaphores[scIndex][subIndex], nullptr);
			myWorkerSystems.back().signalSemaphores[scIndex][subIndex] = myFrameDoneSemaphore[scIndex];
		}
	}
	myWorkerSystemsLocked = true;

	myWorkerSystemsFences = myWorkerSystems.back().system->GetFences();

	int index = 0;
	for (auto& worker : myWorkerSystems)
	{
		myWorkersOrder.emplace_back(index);
		for (auto& feature : worker.system->GetImplementedFeatures())
		{
			myActiveFeatures[feature] = true;
		}
		index++;
	}
	int val = 0;
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

bool
VulkanImplementation::CheckFeature(
	rflx::Features feature)
{
	return myActiveFeatures[feature];
}

void
VulkanImplementation::ToggleFeature(
	rflx::Features feature)
{
	while (vkDeviceWaitIdle(myVulkanFramework.GetDevice()))
	if (feature == rflx::Features::FEATURE_CORE)
	{
		LOG("cannot toggle core features!");
		return;
	}
	myActiveFeatures[feature] = !myActiveFeatures[feature];
	myWorkersOrder.clear();
	int index = 0;
	for (auto& worker : myWorkerSystems)
	{
		const auto implementedFeatures = worker.system->GetImplementedFeatures();
		bool inactiveFeature = false;
		for (auto& implemented : implementedFeatures)
		{
			if (!myActiveFeatures[implemented])
			{
				inactiveFeature = true;
				break;
			}
		}
		if (inactiveFeature)
		{
			index++;
			continue;
		}
		myWorkersOrder.emplace_back(index++);
	}
}

