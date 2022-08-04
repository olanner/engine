
#pragma once
#include "Reflex/VK/Pipelines/Pipeline.h"
#include "Reflex/VK/RenderPass/RenderPass.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"

enum class CubeDimension
{
	Dim2048,
	Dim1024,
	Dim512,
	Dim256,
	Dim128,
	Dim64,
	Dim32,
	Dim16,
	Dim8,
	Dim4,
	Dim2,
	Dim1,

	Count
};

constexpr uint32_t CubeDimValues[uint32_t(CubeDimension::Count)]
{
	2048,
	1024,
	512,
	256,
	128,
	64,
	32,
	16,
	8,
	4,
	2,
	1,
};

struct FilterWork
{
	CubeID id;
	CubeDimension cubeDim;
};

class CubeFilterer : public WorkerSystem
{
public:
													CubeFilterer(
														class VulkanFramework& vulkanFramework,
														class RenderPassFactory& renderPassFactory,
														class ImageAllocator& imageAllocator,
														class ImageHandler& imageHandler,
														QueueFamilyIndex cmdBufferFamily,
														QueueFamilyIndex transferFamily);

	std::tuple<VkSubmitInfo, VkFence>				RecordSubmit(
														uint32_t swapchainImageIndex,
														VkSemaphore* waitSemaphores, 
														uint32_t numWaitSemaphores,
														VkPipelineStageFlags* waitPipelineStages, 
														uint32_t numWaitStages,
														VkSemaphore* signalSemaphore) override;
	std::array<VkFence, NumSwapchainImages>			GetFences() override;

	void											PushFilterWork(
														CubeID			id, 
														CubeDimension	cubeDim);
private:
	void											CreateCubeFilterPass(CubeDimension cubeDim);

	void											RenderFiltering(
														CubeID			id, 
														CubeDimension	filterDim, 
														uint32_t		mip, 
														uint32_t		swapchainIndex);
	void											TransferFilteredCubeMip(
														VkImageView		dstImgView,
														uint32_t		dstMip,
														CubeDimension	cubeDim,
														uint32_t		swapchainIndex);
	void											TransferFilteredCube(
														VkImageView		dstImgView,
														CubeDimension	dstCubeDim,
														uint32_t		swapchainIndex);

	VulkanFramework&								theirVulkanFramework;
	RenderPassFactory&								theirRenderPassFactory;
	ImageAllocator&									theirImageAllocator;
	ImageHandler&									theirImageHandler;

	class Shader*									myFilteringShader;

	VkImageView										myCube;

	std::array<std::array<VkImageView, 6>, uint32_t(CubeDimension::Count)>
													mySplitCubes;
	std::array<Pipeline, uint32_t(CubeDimension::Count)>
													myFilteringPipeline;
	std::array<RenderPass, uint32_t(CubeDimension::Count)>
													myFilteringRenderPass;

	std::array<VkCommandBuffer, NumSwapchainImages>	myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;

	bool											myHasFiltered = false;

	QueueFamilyIndex								myPresentationQueueIndex;

	concurrency::concurrent_queue<FilterWork>		myToDoFilterWork;

};
