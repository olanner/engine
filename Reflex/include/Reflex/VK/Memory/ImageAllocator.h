
#pragma once

#include "AllocatorBase.h"

class ImageAllocator : public AllocatorBase
{
public:
	using											AllocatorBase::AllocatorBase;
													~ImageAllocator();

	std::tuple<VkResult, VkImageView>				RequestImage2D(
														const char*				initialData,
														size_t					initialDataNumBytes,
														uint32_t				width,
														uint32_t				height,
														uint32_t				numMips,
														const QueueFamilyIndex*	firstOwner,
														uint32_t				numOwners,
														VkFormat				format = VK_FORMAT_R8G8B8A8_UNORM,
														VkImageLayout			targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
														VkImageUsageFlags		usage = VK_IMAGE_USAGE_SAMPLED_BIT,
														VkPipelineStageFlags	targetPipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	std::tuple<VkResult, VkImageView>				RequestImageCube(
														const char*				initialData[6],
														size_t					initialDataNumBytes[6],
														uint32_t				width,
														uint32_t				height,
														uint32_t				numMips,
														const QueueFamilyIndex*	firstOwner,
														uint32_t				numOwners,
														VkFormat				format = VK_FORMAT_R8G8B8A8_UNORM,
														VkImageLayout			targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
														VkImageUsageFlags		usage = VK_IMAGE_USAGE_SAMPLED_BIT,
														VkPipelineStageFlags	targetPipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

	std::tuple<VkResult, VkImageView>				RequestImageArray(
														std::vector<char>		initialData,
														uint32_t				numLayers,
														uint32_t				width,
														uint32_t				height,
														uint32_t				numMips,
														const QueueFamilyIndex*	firstOwner,
														uint32_t				numOwners,
														VkFormat				format = VK_FORMAT_R8G8B8A8_UNORM,
														VkImageLayout			targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
														VkImageUsageFlags		usage = VK_IMAGE_USAGE_SAMPLED_BIT,
														VkPipelineStageFlags	targetPipelineStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	
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
														VkImage					image,
														uint32_t				width,
														uint32_t				height,
														uint32_t				layer,
														const char*				data,
														uint64_t				numBytes,
														const QueueFamilyIndex*	firstOwner,
														uint32_t				numOwners);
	void											RecordBlit(
														VkImage		image,
														uint32_t	baseWidth,
														uint32_t	baseHeight,
														uint32_t	numMips,
														uint32_t	layer = 0);
	void											RecordImageTransition(
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
