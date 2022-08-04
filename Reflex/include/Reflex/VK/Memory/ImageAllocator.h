
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

class ImageAllocator : public AllocatorBase
{
public:
	using											AllocatorBase::AllocatorBase;
													~ImageAllocator();

	std::tuple<VkResult, VkImageView>				RequestImage2D(
														AllocationSubmission&	allocSub,
														const uint8_t*			initialData,
														size_t					initialDataNumBytes,
														const ImageRequestInfo&	requestInfo);

	std::tuple<VkResult, VkImageView>				RequestImageCube(
														AllocationSubmission&	allocSub,
														std::vector<uint8_t>	initialData,
														size_t					initialDataBytesPerLayer,
														const ImageRequestInfo&	requestInfo);

	std::tuple<VkResult, VkImageView>				RequestImageArray(
														AllocationSubmission&	allocSub,
														std::vector<uint8_t>	initialData, 
														uint32_t				numLayers, 
														const ImageRequestInfo& requestInfo);
	
	void											SetDebugName(
														VkImageView	imgView, 
														const char*	name);
	VkImage											GetImage(
														VkImageView imgView);


private:
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
														StagingBuffer&			outStagingBuffer,
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

	std::unordered_map<VkImage, VkDeviceMemory>		myAllocatedImages;
	std::unordered_map<VkImageView, VkImage>		myImageViews;

	VkClearColorValue								myClearColor
	{
		{1,1,0,1},
	};

};
