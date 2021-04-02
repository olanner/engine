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
	//SAFE_DELETE( myBaseShader );

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
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myHasTransferredImagesSemaphore[i], nullptr);
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myHasTransferredBuffersSemaphore[i], nullptr);
		vkDestroySemaphore(myVulkanFramework.GetDevice(), myHasTransferredAccStructsSemaphore[i], nullptr);
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
}

VkResult
VulkanImplementation::Init(
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

	myBufferAllocator = std::make_unique<BufferAllocator>(myVulkanFramework, *myImmediateTransferrer, transQueueFamily);
	myImageAllocator = std::make_unique<ImageAllocator>(myVulkanFramework, *myImmediateTransferrer, transQueueFamily);
	myAccelerationStructureAllocator = std::make_unique<AccelerationStructureAllocator>(myVulkanFramework,
																		   *myBufferAllocator,
																		   *myImmediateTransferrer,
																		   transQueueFamily,
																		   presQueueFamily);

	myRenderPassFactory = std::make_unique<RenderPassFactory>(myVulkanFramework,
												 *myImageAllocator,
												 myQueueFamilyIndices.data(),
												 myQueueFamilyIndices.size());

	//VkFormat colFormats[]
	//{
	//	VK_FORMAT_R8G8B8A8_SRGB,
	//	VK_FORMAT_R8G8B8A8_SRGB,
	//	VK_FORMAT_R32G32B32A32_SFLOAT,
	//	VK_FORMAT_R32G32B32A32_SFLOAT,
	//};
	//myRenderPassFactory->RequestRenderPass( 4,
	//										colFormats,
	//										{ 1920,1080 },
	//										int( RenderPassRequestFlags::UseDepthAttachment ) | int( RenderPassRequestFlags::BuildFramebuffer )
	//);

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
									*myImageAllocator,
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

	VkResult resultSemaphore;
	for (uint32_t i = 0; i < NumSwapchainImages; ++i)
	{
		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myImageAvailableSemaphore[i]);
		DebugSetObjectName(std::string("Image Available Sem " + std::to_string(i)).c_str(), myImageAvailableSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);

		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myFrameDoneSemaphore[i]);
		DebugSetObjectName(std::string("Has Rendered Sem " + std::to_string(i)).c_str(), myFrameDoneSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);

		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myHasTransferredImagesSemaphore[i]);
		DebugSetObjectName(std::string("Has Transferred Images Sem " + std::to_string(i)).c_str(), myHasTransferredImagesSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);

		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myHasTransferredBuffersSemaphore[i]);
		DebugSetObjectName(std::string("Has Transferred Buffers Sem " + std::to_string(i)).c_str(), myHasTransferredBuffersSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);

		resultSemaphore = vkCreateSemaphore(myVulkanFramework.GetDevice(), &semaphoreInfo, nullptr, &myHasTransferredAccStructsSemaphore[i]);
		DebugSetObjectName(std::string("Has Transferred Acc Structs Sem " + std::to_string(i)).c_str(), myHasTransferredAccStructsSemaphore[i], VK_OBJECT_TYPE_SEMAPHORE, myVulkanFramework.GetDevice());
		VK_FALLTHROUGH(resultSemaphore);
	}

	return VK_SUCCESS;
}

void
VulkanImplementation::SubmitTransferCmds()
{
	// TRANSFER IMAGES SUBMIT
	auto [cmdBufferImages, cmdFenceImages] = myImageAllocator->AcquireNextTransferBuffer();

	VkSubmitInfo transferImagesSubmitInfo{};
	transferImagesSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transferImagesSubmitInfo.pNext = nullptr;

	transferImagesSubmitInfo.commandBufferCount = 1;
	transferImagesSubmitInfo.pCommandBuffers = &cmdBufferImages;
	transferImagesSubmitInfo.signalSemaphoreCount = 1;
	transferImagesSubmitInfo.pSignalSemaphores = &myHasTransferredImagesSemaphore[mySwapchainImageIndex];

	auto resultReset = vkResetFences(myVulkanFramework.GetDevice(), 1, &cmdFenceImages);
	VkResult resultSubmit = vkQueueSubmit(myTransferQueue, 1, &transferImagesSubmitInfo, cmdFenceImages);
	assert(!resultSubmit && "failed submission");

	// TRANSFER BUFFERS SUBMIT

	auto [cmdBufferTransferBuffers, cmdFenceTransferBuffers] = myBufferAllocator->AcquireNextTransferBuffer();

	VkSubmitInfo transferSubmitInfo{};
	transferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transferSubmitInfo.pNext = nullptr;

	transferSubmitInfo.commandBufferCount = 1;
	transferSubmitInfo.pCommandBuffers = &cmdBufferTransferBuffers;
	transferSubmitInfo.signalSemaphoreCount = 1;
	transferSubmitInfo.pSignalSemaphores = &myHasTransferredBuffersSemaphore[mySwapchainImageIndex];

	vkResetFences(myVulkanFramework.GetDevice(), 1, &cmdFenceTransferBuffers);
	resultSubmit = vkQueueSubmit(myTransferQueue, 1, &transferSubmitInfo, cmdFenceTransferBuffers);
	assert(!resultSubmit && "failed submission");

	// TRANSFER ACC STRUCTS SUBMIT
	auto [cmdBufferTransferAccStructs, cmdFenceTransferAccStructs] = myAccelerationStructureAllocator->AcquireNextTransferBuffer();

	VkSubmitInfo transferAccStructsSubmitInfo{};
	transferAccStructsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	transferAccStructsSubmitInfo.pNext = nullptr;

	transferAccStructsSubmitInfo.commandBufferCount = 1;
	transferAccStructsSubmitInfo.pCommandBuffers = &cmdBufferTransferAccStructs;
	transferAccStructsSubmitInfo.signalSemaphoreCount = 1;
	transferAccStructsSubmitInfo.pSignalSemaphores = &myHasTransferredAccStructsSemaphore[mySwapchainImageIndex];

	vkResetFences(myVulkanFramework.GetDevice(), 1, &cmdFenceTransferAccStructs);
	resultSubmit = vkQueueSubmit(myTransferQueue, 1, &transferAccStructsSubmitInfo, cmdFenceTransferAccStructs);
	assert(!resultSubmit && "failed submission");
}

void
VulkanImplementation::SubmitWorkerCmds()
{
	for (auto& wSys : myWorkerSystems)
	{
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
		toSlot.AddWaitSemaphore(myHasTransferredImagesSemaphore, waitStage);
		toSlot.AddWaitSemaphore(myHasTransferredBuffersSemaphore, waitStage);
		toSlot.AddWaitSemaphore(myHasTransferredAccStructsSemaphore, waitStage);
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
}

