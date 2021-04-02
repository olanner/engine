#include "pch.h"
#include "ImageAllocator.h"

#include <numeric>

#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Debug/DebugUtils.h"

ImageAllocator::~ImageAllocator()
{
	for (auto& [image, memory] : myAllocatedImages)
	{
		if (image)
		{
			vkDestroyImage(theirVulkanFramework.GetDevice(), image, nullptr);
		}
		if (memory)
		{
			vkFreeMemory(theirVulkanFramework.GetDevice(), memory, nullptr);
		}
	}
	for (auto& [view, image] : myImageViews)
	{
		if (view)
		{
			vkDestroyImageView(theirVulkanFramework.GetDevice(), view, nullptr);
		}
	}
}

std::tuple<VkResult, VkImageView>
ImageAllocator::RequestImage2D(
	const char*				initialData,
	size_t					initialDataNumBytes,
	uint32_t				width,
	uint32_t				height,
	uint32_t				numMips,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners,
	VkFormat				format,
	VkImageLayout			targetLayout,
	VkImageUsageFlags		usage,
	VkPipelineStageFlags	targetPipelineStage)
{
	VkImage image{};
	VkImageView view{};
	VkDeviceMemory memory{};

	assert(!(initialData && !initialDataNumBytes) && "invalid operation : requesting image with valid data with invalid number of bytes");

	// IMAGE
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.flags = NULL;

	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = numMips;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage;

	if (IsExclusive(firstOwner, numOwners))
	{
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.pQueueFamilyIndices = firstOwner;
		imageInfo.queueFamilyIndexCount = 1;
	}
	else
	{
		imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageInfo.pQueueFamilyIndices = firstOwner;
		imageInfo.queueFamilyIndexCount = numOwners;
	}

	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	auto resultImage = vkCreateImage(theirVulkanFramework.GetDevice(), &imageInfo, nullptr, &image);
	if (resultImage)
	{
		LOG("failed creating image");
		return {resultImage, nullptr};
	}

	// MEMORY
	auto [memReq, typeIndex] = GetMemReq(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (typeIndex == UINT_MAX)
	{
		return {VK_ERROR_FEATURE_NOT_PRESENT, nullptr};
	}
	assert(initialDataNumBytes <= memReq.size && "byte size of image layer 0 mip 0 is too large");

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = typeIndex;


	auto resultMem = vkAllocateMemory(theirVulkanFramework.GetDevice(), &allocInfo, nullptr, &memory);
	if (resultMem)
	{
		LOG("failed to allocate memory");
		return {resultMem, nullptr};
	}

	vkBindImageMemory(theirVulkanFramework.GetDevice(), image, memory, 0);

	// FIRST IMAGE ALLOC

	if (initialData)
	{
		RecordImageAlloc(image, width, height, 0, initialData, initialDataNumBytes, firstOwner, numOwners);
		RecordBlit(image, width, height, numMips, 0);
		RecordImageTransition(image, numMips, 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetLayout, targetPipelineStage);
	}
	else
	{
		RecordImageTransition(image, numMips, 1, VK_IMAGE_LAYOUT_UNDEFINED, targetLayout, targetPipelineStage);
	}

	// TRANSFER TO TARGET LAYOUT
	//for ( uint mipLevel = 0; mipLevel < numMips; ++mipLevel )


	// VIEW
	VkImageViewCreateInfo  viewInfo;
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.pNext = nullptr;
	viewInfo.flags = NULL;

	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = EvaluateImageAspect(targetLayout);
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = numMips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	auto resultView = vkCreateImageView(theirVulkanFramework.GetDevice(), &viewInfo, nullptr, &view);
	if (resultView)
	{
		LOG("failed creating image view");
		return {resultView, nullptr};
	}

	myAllocatedImages[image] = memory;
	myImageViews[view] = image;

	return {VK_SUCCESS, view};
}

std::tuple<VkResult, VkImageView>
ImageAllocator::RequestImageCube(
	const char*				initialData[6],
	size_t					initialDataNumBytes[6],
	uint32_t				width,
	uint32_t				height,
	uint32_t				numMips,
	const QueueFamilyIndex*	firstOwner,
	uint32_t				numOwners,
	VkFormat				format,
	VkImageLayout			targetLayout,
	VkImageUsageFlags		usage,
	VkPipelineStageFlags	targetPipelineStage)
{
	VkImage image{};
	VkImageView view{};
	VkDeviceMemory memory{};

	assert(!(initialData && !initialDataNumBytes) && "invalid operation : requesting image with valid data with invalid number of bytes");

	// IMAGE
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = numMips;
	imageInfo.arrayLayers = 6;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage;

	if (IsExclusive(firstOwner, numOwners))
	{
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.pQueueFamilyIndices = firstOwner;
		imageInfo.queueFamilyIndexCount = 1;
	}
	else
	{
		imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageInfo.pQueueFamilyIndices = firstOwner;
		imageInfo.queueFamilyIndexCount = numOwners;
	}

	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	auto resultImage = vkCreateImage(theirVulkanFramework.GetDevice(), &imageInfo, nullptr, &image);
	if (resultImage)
	{
		LOG("failed creating image");
		return {resultImage, nullptr};
	}

	// MEMORY
	auto [memReq, typeIndex] = GetMemReq(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (typeIndex == UINT_MAX)
	{
		return {VK_ERROR_FEATURE_NOT_PRESENT, nullptr};
	}
	if (initialData)
	{
		assert(std::accumulate(initialDataNumBytes, initialDataNumBytes + 6, 0) <= memReq.size && "byte size of image layer 0 mip 0 is too large");
	}

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = typeIndex;


	auto resultMem = vkAllocateMemory(theirVulkanFramework.GetDevice(), &allocInfo, nullptr, &memory);
	if (resultMem)
	{
		LOG("failed to allocate memory");
		return {resultMem, nullptr};
	}

	vkBindImageMemory(theirVulkanFramework.GetDevice(), image, memory, 0);

	// FIRST IMAGE ALLOC

	if (initialData)
	{
		for (int i = 0; i < 6; ++i)
		{
			RecordImageAlloc(image, width, height, i, initialData[i], initialDataNumBytes[i], firstOwner, numOwners);
		}
		for (int i = 0; i < 6; ++i)
		{
			RecordBlit(image, width, height, numMips, i);
		}
		RecordImageTransition(image, numMips, 6, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetLayout, targetPipelineStage);
	}
	else
	{
		RecordImageTransition(image, numMips, 6, VK_IMAGE_LAYOUT_UNDEFINED, targetLayout, targetPipelineStage);
	}

	// VIEW
	VkImageViewCreateInfo  viewInfo;
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.pNext = nullptr;
	viewInfo.flags = NULL;

	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewInfo.format = format;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = numMips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 6;

	auto resultView = vkCreateImageView(theirVulkanFramework.GetDevice(), &viewInfo, nullptr, &view);
	if (resultView)
	{
		LOG("failed creating image view");
		return {resultView, nullptr};
	}

	myAllocatedImages[image] = memory;
	myImageViews[view] = image;

	return {VK_SUCCESS, view};
}

std::tuple<VkResult, VkImageView>
ImageAllocator::RequestImageArray(
	std::vector<char>				initialData,
	uint32_t						numLayers,
	uint32_t						width,
	uint32_t						height,
	uint32_t						numMips,
	const QueueFamilyIndex*			firstOwner,
	uint32_t						numOwners,
	VkFormat						format,
	VkImageLayout					targetLayout,
	VkImageUsageFlags				usage, 
	VkPipelineStageFlags			targetPipelineStage)
{
	VkImage image{};
	VkImageView view{};
	VkDeviceMemory memory{};

	// IMAGE
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.flags = NULL;

	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = numMips;
	imageInfo.arrayLayers = numLayers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage;

	if (IsExclusive(firstOwner, numOwners))
	{
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.pQueueFamilyIndices = firstOwner;
		imageInfo.queueFamilyIndexCount = 1;
	}
	else
	{
		imageInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageInfo.pQueueFamilyIndices = firstOwner;
		imageInfo.queueFamilyIndexCount = numOwners;
	}

	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	auto resultImage = vkCreateImage(theirVulkanFramework.GetDevice(), &imageInfo, nullptr, &image);
	if (resultImage)
	{
		LOG("failed creating image");
		return {resultImage, nullptr};
	}

	// MEMORY
	auto [memReq, typeIndex] = GetMemReq(image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (typeIndex == UINT_MAX)
	{
		return {VK_ERROR_FEATURE_NOT_PRESENT, nullptr};
	}
	assert(initialData.size() <= memReq.size && "byte size of image layer 0 mip 0 is too large");

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = typeIndex;


	auto resultMem = vkAllocateMemory(theirVulkanFramework.GetDevice(), &allocInfo, nullptr, &memory);
	if (resultMem)
	{
		LOG("failed to allocate memory");
		return {resultMem, nullptr};
	}

	vkBindImageMemory(theirVulkanFramework.GetDevice(), image, memory, 0);

	// IMAGE ALLOC
	if (!initialData.empty())
	{
		const uint64_t numImgBytes = initialData.size() / numLayers;
		uint64_t byteOffset = 0;
		for (uint32_t layer = 0; layer < numLayers; ++layer)
		{
			RecordImageAlloc(image, width, height, layer, initialData.data() + byteOffset, numImgBytes, firstOwner, numOwners);
			byteOffset += numImgBytes;
		}
		for (uint32_t layer = 0; layer < numLayers; ++layer)
		{
			RecordBlit(image, width, height, numMips, layer);
		}
		RecordImageTransition(image, numMips, numLayers, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetLayout, targetPipelineStage);
	}
	else
	{
		RecordImageTransition(image, numMips, numLayers, VK_IMAGE_LAYOUT_UNDEFINED, targetLayout, targetPipelineStage);
	}

	// VIEW
	VkImageViewCreateInfo viewInfo;
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.pNext = nullptr;
	viewInfo.flags = NULL;

	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	viewInfo.format = format;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = EvaluateImageAspect(targetLayout);
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = numMips;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = numLayers;

	auto resultView = vkCreateImageView(theirVulkanFramework.GetDevice(), &viewInfo, nullptr, &view);
	if (resultView)
	{
		LOG("failed creating image view");
		return {resultView, nullptr};
	}

	myAllocatedImages[image] = memory;
	myImageViews[view] = image;

	return {VK_SUCCESS, view};
}

void
ImageAllocator::SetDebugName(
	VkImageView imgView, 
	const char* name)
{
	DebugSetObjectName(std::string(name).append("_view").c_str(), imgView, VK_OBJECT_TYPE_IMAGE_VIEW, theirVulkanFramework.GetDevice());
	DebugSetObjectName(std::string(name).append("_img").c_str(), myImageViews[imgView], VK_OBJECT_TYPE_IMAGE, theirVulkanFramework.GetDevice());
}

VkImage
ImageAllocator::GetImage(
	VkImageView imgView)
{
	return myImageViews[imgView];
}

VkImageMemoryBarrier
ImageAllocator::CreateTransition(
	VkImage					image,
	VkImageSubresourceRange range,
	VkImageLayout			prevLayout,
	VkImageLayout			nextLayout)
{

	VkImageMemoryBarrier memBarrier{};
	memBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	memBarrier.pNext = nullptr;

	memBarrier.srcAccessMask = EvaluateAccessFlags(prevLayout);
	memBarrier.dstAccessMask = EvaluateAccessFlags(nextLayout);

	memBarrier.oldLayout = prevLayout;
	memBarrier.newLayout = nextLayout;
	memBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	memBarrier.image = image;
	memBarrier.subresourceRange = range;

	return memBarrier;;
}

VkAccessFlags
ImageAllocator::EvaluateAccessFlags(
	VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return NULL;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT;

		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

		default:
			assert(false && "undefined image transition");
	}
	return NULL;
}

VkImageAspectFlags
ImageAllocator::EvaluateImageAspect(
	VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_IMAGE_ASPECT_COLOR_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_IMAGE_ASPECT_COLOR_BIT;

		default:
			assert(false && "unimplementend image layout");
			return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

void
ImageAllocator::RecordImageAlloc(
	VkImage					image,
	uint32_t				width,
	uint32_t				height,
	uint32_t				layer,
	const char*				data,
	uint64_t				numBytes,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners)
{
	{
		VkBufferImageCopy copy{};
		copy.bufferOffset = 0;
		copy.bufferRowLength = 0;
		copy.bufferImageHeight = 0;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = layer;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset.x = 0;
		copy.imageOffset.y = 0;
		copy.imageOffset.z = 0;
		copy.imageExtent.width = width;
		copy.imageExtent.height = height;
		copy.imageExtent.depth = 1;

		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseMipLevel = 0;
		range.levelCount = 1;
		range.baseArrayLayer = layer;
		range.layerCount = 1;
		auto undefToDest = CreateTransition(image, range, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		auto destToSrc = CreateTransition(image, range, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		std::scoped_lock<std::mutex> lock(myTransferMutex);
		vkCmdPipelineBarrier(myTransferBuffers[myTransferIndex],
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  NULL,
							  0,
							  nullptr,
							  0,
							  nullptr,
							  1,
							  &undefToDest);
		if (data)
		{
			auto [resultStaged, stagedBuffer, stagedMemory] = CreateStagingBuffer(data, numBytes, firstOwner, numOwners);
			vkCmdCopyBufferToImage(myTransferBuffers[myTransferIndex], stagedBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		}
		else
		{
			vkCmdClearColorImage(myTransferBuffers[myTransferIndex], image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &myClearColor, 1, &range);
		}
		vkCmdPipelineBarrier(myTransferBuffers[myTransferIndex],
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  NULL,
							  0,
							  nullptr,
							  0,
							  nullptr,
							  1,
							  &destToSrc);
	}
}

void
ImageAllocator::RecordBlit(
	VkImage		image,
	uint32_t	baseWidth,
	uint32_t	baseHeight,
	uint32_t	numMips,
	uint32_t	layer)
{
	// BLIT
	// UNDEF -> DST -> SRC
	int srcWidth = baseWidth;
	int srcHeight = baseHeight;
	int dstWidth = baseWidth / 2;
	int dstHeight = baseHeight / 2;
	for (uint32_t mipLevel = 1; mipLevel < numMips; ++mipLevel)
	{
		// BARRIERS
		VkImageSubresourceRange srcRange{};
		srcRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		srcRange.baseMipLevel = mipLevel - 1;
		srcRange.levelCount = 1;
		srcRange.baseArrayLayer = layer;
		srcRange.layerCount = 1;

		VkImageSubresourceRange dstRange{};
		dstRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		dstRange.baseMipLevel = mipLevel;
		dstRange.levelCount = 1;
		dstRange.baseArrayLayer = layer;
		dstRange.layerCount = 1;

		auto undefToDest = CreateTransition(image, dstRange, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		auto destToSrc = CreateTransition(image, dstRange, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		VkImageBlit blit{};
		blit.srcOffsets[0] = {0,0,0};
		blit.srcOffsets[1] = {srcWidth,srcHeight,1};
		blit.srcSubresource.mipLevel = mipLevel - 1;
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.baseArrayLayer = layer;
		blit.srcSubresource.layerCount = 1;

		blit.dstOffsets[0] = {0,0,0};
		blit.dstOffsets[1] = {dstWidth, dstHeight,1};
		blit.dstSubresource.mipLevel = mipLevel;
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.baseArrayLayer = layer;
		blit.dstSubresource.layerCount = 1;

		// TRANSFER WORK
		std::scoped_lock<std::mutex> lock(myTransferMutex);
		vkCmdPipelineBarrier(myTransferBuffers[myTransferIndex],
							  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  NULL,
							  0,
							  nullptr,
							  0,
							  nullptr,
							  1,
							  &undefToDest);
		vkCmdBlitImage(myTransferBuffers[myTransferIndex],
						image,
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						image,
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&blit,
						VK_FILTER_LINEAR);
		vkCmdPipelineBarrier(myTransferBuffers[myTransferIndex],
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  NULL,
							  0,
							  nullptr,
							  0,
							  nullptr,
							  1,
							  &destToSrc);
		srcHeight /= 2;
		srcWidth /= 2;
		dstHeight /= 2;
		dstWidth /= 2;
	}
}

void
ImageAllocator::RecordImageTransition(
	VkImage					image,
	uint32_t				numMips,
	uint32_t				numLayers,
	VkImageLayout			prevLayout,
	VkImageLayout			targetLayout,
	VkPipelineStageFlags	targetPipelineStage)
{
	// BARRIER
	VkImageSubresourceRange range;
	range.aspectMask = EvaluateImageAspect(targetLayout);
	range.baseMipLevel = 0;
	range.levelCount = numMips;
	range.baseArrayLayer = 0;
	range.layerCount = numLayers;

	auto destToRead =
		CreateTransition(image,
						  range,
						  prevLayout,
						  targetLayout);

	std::scoped_lock<std::mutex> lock(myTransferMutex);
	vkCmdPipelineBarrier(myTransferBuffers[myTransferIndex],
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  targetPipelineStage,
						  NULL,
						  0,
						  nullptr,
						  0,
						  nullptr,
						  1,
						  &destToRead);
}


