
#pragma once

#include "AllocatorBase.h"

struct ImageRequestInfo
{
	int								width				= 0;
	int								height				= 0;
	int								mips				= 1;
	std::vector<QueueFamilyIndex>	owners				= {};
	VkFormat						format				= VK_FORMAT_R8G8B8A8_UNORM;
	VkImageLayout					layout				= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkImageUsageFlags				usage				= VK_IMAGE_USAGE_SAMPLED_BIT;
	VkPipelineStageFlags			targetPipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
};

struct AllocatedImage
{
	VkImageView		view	= nullptr;
	VkImage			image	= nullptr;
	VkDeviceMemory	memory	= nullptr;
};

struct QueuedImageDestroy
{
	VkImageView														imageView = nullptr;
	int																tries = 0;
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	waitSignal;
};

class ImageAllocator : public AllocatorBase
{
public:
	using											AllocatorBase::AllocatorBase;
													~ImageAllocator();

	std::tuple<VkResult, VkImageView>				RequestImage2D(
														AllocationSubmissionID	allocSubID,
														const uint8_t*			initialData,
														size_t					initialDataNumBytes,
														const ImageRequestInfo&	requestInfo);

	std::tuple<VkResult, VkImageView>				RequestImageCube(
														AllocationSubmissionID	allocSubID,
														std::vector<uint8_t>	initialData,
														size_t					initialDataBytesPerLayer,
														const ImageRequestInfo&	requestInfo);

	std::tuple<VkResult, VkImageView>				RequestImageArray(
														AllocationSubmissionID	allocSubID,
														std::vector<uint8_t>	initialData, 
														uint32_t				numLayers, 
														const ImageRequestInfo& requestInfo);

	void											QueueDestroy(
														VkImageView&&													imageView,
														std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	waitSignal);
	void											DoCleanUp(int limit) override;
	
	void											SetDebugName(
														VkImageView	imgView, 
														const char*	name);
	VkImage											GetImage(
														VkImageView imgView);


private:
	void											QueueRequest(
														AllocationSubmissionID	allocSubID,
														AllocatedImage			toQueue);
	
	VkImageMemoryBarrier							CreateTransition(
														VkImage					image,
														VkImageSubresourceRange	range,
														VkImageLayout			prevLayout, 
														VkImageLayout			nextLayout);
	VkAccessFlags									EvaluateAccessFlags(
														VkImageLayout layout);
	VkImageAspectFlags								EvaluateImageAspect(
														VkImageLayout layout);

	void											RecordImageAlloc(
														VkCommandBuffer			cmdBuffer,
														BufferXMemory&			outStagingBuffer,
														VkImage					image,
														uint32_t				width,
														uint32_t				height,
														uint32_t				layer,
														const uint8_t*			data,
														uint64_t				numBytes,
														const QueueFamilyIndex*	firstOwner, 
														uint32_t				numOwners);
	void											RecordBlit(
														VkCommandBuffer			cmdBuffer,	
														VkImage					image,
														uint32_t				baseWidth,
														uint32_t				baseHeight,
														uint32_t				numMips,
														uint32_t				layer = 0);
	void											RecordImageTransition(
														VkCommandBuffer			cmdBuffer,	
														VkImage					image,
														uint32_t				numMips,
														uint32_t				numLayers = 0,
														VkImageLayout			prevLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
														VkImageLayout			targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
														VkPipelineStageFlags	targetPipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	//conc_map<VkImageView, VkImage>					myImageViews;
	//conc_map<VkImage, VkDeviceMemory>				myAllocatedImages;
	std::unordered_map<VkImageView, AllocatedImage>	myAllocatedImages;

	conc_queue<AllocatedImage>						myRequestedImagesQueue;
	conc_queue<QueuedImageDestroy>					myImageDestroyQueue;
	std::vector<QueuedImageDestroy>					myFailedDestructs;

	VkClearColorValue								myClearColor
	{
		{1,1,0,1},
	};

};
