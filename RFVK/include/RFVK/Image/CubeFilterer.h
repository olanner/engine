
#pragma once
#include "RFVK/Pipelines/Pipeline.h"
#include "RFVK/RenderPass/RenderPass.h"
#include "RFVK/WorkerSystem/WorkerSystem.h"

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
	CubeID									id;
	CubeDimension							cubeDim;
	shared_semaphore<NumSwapchainImages>	canFilter;
};

class CubeFilterer : public WorkerSystem
{
public:
													CubeFilterer(
														class VulkanFramework&		vulkanFramework,
														class RenderPassFactory&	renderPassFactory,
														class ImageAllocator&		imageAllocator,
														class ImageHandler&			imageHandler,
														QueueFamilyIndices			familyIndices);

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
													RecordSubmit(
														uint32_t swapchainImageIndex, 
														const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores, 
														const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores) override;
	std::array<VkFence, NumSwapchainImages>			GetFences() override;
	std::vector<rflx::Features>						GetImplementedFeatures() const override;
	int												GetSubmissionCount() override { return 1; }

	void											PushFilterWork(FilterWork&& filterWork);
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

	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
													myWaitStages;
	class Shader*									myFilteringShader;
	std::array<Pipeline, uint32_t(CubeDimension::Count)>
													myFilteringPipeline;
	std::array<RenderPass, uint32_t(CubeDimension::Count)>
													myFilteringRenderPass;
	std::array<std::array<VkImageView, 6>, uint32_t(CubeDimension::Count)>
													mySplitCubes;
	VkImageView										myCube;

	std::array<VkCommandBuffer, NumSwapchainImages>	myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;

	bool											myHasFiltered = false;

	QueueFamilyIndex								myPresentationQueueIndex;

	concurrency::concurrent_queue<FilterWork>		myToDoFilterWork;

};
