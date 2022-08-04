#include "pch.h"
#include "CubeFilterer.h"


#include "ImageHandler.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/ImageAllocator.h"
#include "Reflex/VK/Pipelines/PipelineBuilder.h"
#include "Reflex/VK/RenderPass/RenderPassFactory.h"
#include "Reflex/VK/Shader/Shader.h"

CubeFilterer::CubeFilterer(
	VulkanFramework& vulkanFramework,
	RenderPassFactory& renderPassFactory,
	ImageAllocator& imageAllocator,
	ImageHandler& imageHandler,
	QueueFamilyIndex	cmdBufferFamily,
	QueueFamilyIndex	transferFamily)
	: theirVulkanFramework(vulkanFramework)
	, theirRenderPassFactory(renderPassFactory)
	, theirImageAllocator(imageAllocator)
	, theirImageHandler(imageHandler)
{
	QueueFamilyIndex qIndices[]{myPresentationQueueIndex = cmdBufferFamily, transferFamily};

	auto allocSub = theirImageAllocator.Start();
	size_t sizes[6]{};
	VkResult result;

	ImageRequestInfo requestInfo;
	requestInfo.width = 2048;
	requestInfo.height = 2048;
	requestInfo.mips = NUM_MIPS(2048);
	requestInfo.owners = {cmdBufferFamily, transferFamily};
	requestInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	requestInfo.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	requestInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	requestInfo.targetPipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	std::tie(result, myCube) =
		theirImageAllocator.RequestImageCube(
		allocSub,
		{},
		0,
		requestInfo);

	theirImageAllocator.Queue(std::move(allocSub));
	
	assert(!result && "failed creating cube");

	char shaderPaths[][128]
	{
		"Shaders/cube_filter_vshader.vert",
		"Shaders/cube_filter_fshader.frag"
	};
	myFilteringShader = new Shader(shaderPaths, _ARRAYSIZE(shaderPaths), theirVulkanFramework);

	for (uint32_t cubeDimIndex = 0; cubeDimIndex < uint32_t(CubeDimension::Count); ++cubeDimIndex)
	{
		CreateCubeFilterPass(CubeDimension(cubeDimIndex));
	}

	// COMMAND BUFFERS
	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		auto [result, cmdBuffer] = theirVulkanFramework.RequestCommandBuffer(cmdBufferFamily);
		assert(!result && "failed command buffer request");
		myCmdBuffers[scIndex] = cmdBuffer;
	}

	// FENCES
	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		auto resultFence = vkCreateFence(theirVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myCmdBufferFences[i]);
		assert(!resultFence && "failed creating fences");
	}

}

std::tuple<VkSubmitInfo, VkFence>
CubeFilterer::RecordSubmit(
	uint32_t				swapchainImageIndex,
	VkSemaphore* waitSemaphores,
	uint32_t				numWaitSemaphores,
	VkPipelineStageFlags* waitPipelineStages,
	uint32_t				numWaitStages,
	VkSemaphore* signalSemaphore)
{
	// RECORD
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;

	beginInfo.pInheritanceInfo = nullptr;

	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
	}
	auto resultBegin = vkBeginCommandBuffer(myCmdBuffers[swapchainImageIndex], &beginInfo);



	FilterWork work{};
	bool popped = myToDoFilterWork.try_pop(work);
	if (popped)
	{
		for (uint32_t dim = uint32_t(work.cubeDim); dim < uint32_t(CubeDimension::Count); ++dim)
		{
			uint32_t mip = dim - uint32_t(work.cubeDim);
			RenderFiltering(work.id, CubeDimension(dim), mip, swapchainImageIndex);
			TransferFilteredCubeMip(theirImageHandler[work.id].view, mip, CubeDimension(dim), swapchainImageIndex);
		}
	}

	if (!myHasFiltered)
	{

		myHasFiltered = true;
	}

	auto resultEnd = vkEndCommandBuffer(myCmdBuffers[swapchainImageIndex]);

	if (resultBegin || resultEnd)
	{
		LOG("CubeFilterer failed recording");
	}

	// FILL IN SUBMISSION
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submitInfo.pWaitDstStageMask = waitPipelineStages;

	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.waitSemaphoreCount = numWaitSemaphores;
	submitInfo.pCommandBuffers = &myCmdBuffers[swapchainImageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphore;
	submitInfo.signalSemaphoreCount = signalSemaphore != nullptr;

	return {submitInfo, myCmdBufferFences[swapchainImageIndex]};
}

std::array<VkFence, NumSwapchainImages> CubeFilterer::GetFences()
{
    return myCmdBufferFences;
}

void
CubeFilterer::PushFilterWork(
	CubeID			id,
	CubeDimension	cubeDim)
{
	myToDoFilterWork.push({id, cubeDim});
}

void
CubeFilterer::CreateCubeFilterPass(CubeDimension cubeDim)
{
	auto cubeImage = theirImageAllocator.GetImage(myCube);
	uint32_t res = CubeDimValues[uint32_t(cubeDim)];

	VkResult result;
	for (int sliceIndex = 0; sliceIndex < 6; ++sliceIndex)
	{
		VkImageViewCreateInfo  cubeSliceInfo{};
		cubeSliceInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

		cubeSliceInfo.image = cubeImage;
		cubeSliceInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		cubeSliceInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		cubeSliceInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		cubeSliceInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		cubeSliceInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		cubeSliceInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		cubeSliceInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		cubeSliceInfo.subresourceRange.baseMipLevel = uint32_t(cubeDim);
		cubeSliceInfo.subresourceRange.levelCount = 1;
		cubeSliceInfo.subresourceRange.baseArrayLayer = sliceIndex;
		cubeSliceInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(theirVulkanFramework.GetDevice(), &cubeSliceInfo, nullptr, &mySplitCubes[uint32_t(cubeDim)][sliceIndex]);
		assert(!result && "failed creating split cube image view");
	}

	// RENDER PASS
	auto rpBuilder = theirRenderPassFactory.GetConstructor();
	rpBuilder.DefineAttachments({
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UNORM}, {res, res});

	rpBuilder.AddSubpass()
		.SetColorAttachments({0,1,2,3,4,5});

	rpBuilder.AddSubpassDependency({
		VK_SUBPASS_EXTERNAL,
		0,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
								   });

	rpBuilder.AddSubpassDependency({
		0,
		VK_SUBPASS_EXTERNAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
								   });

	for (int sliceIndex = 0; sliceIndex < 6; ++sliceIndex)
	{
		rpBuilder.EditAttachmentDescription(sliceIndex).finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		rpBuilder.SetImageViews(sliceIndex, mySplitCubes[uint32_t(cubeDim)][sliceIndex]);
	}

	myFilteringRenderPass[uint32_t(cubeDim)] = rpBuilder.Build();

	// PIPELINE


	PipelineBuilder pBuilder(6, myFilteringRenderPass[uint32_t(cubeDim)].renderPass, myFilteringShader);

	pBuilder.DefineVertexInput(nullptr)
		.DefineViewport({res,res}, {0,0,res,res})
		.SetAllBlendStates(GenBlendState::Disabled)
		.SetDepthEnabled(false);
	std::tie(result, myFilteringPipeline[uint32_t(cubeDim)]) = pBuilder.Construct({
		theirImageHandler.GetSamplerSetLayout(),
		theirImageHandler.GetImageSetLayout()
																				  }, theirVulkanFramework.GetDevice());
}

void
CubeFilterer::RenderFiltering(
	CubeID			id,
	CubeDimension	filterDim,
	uint32_t		mip,
	uint32_t		swapchainIndex)
{
	uint32_t res = CubeDimValues[uint32_t(filterDim)];

	BeginRenderPass(myCmdBuffers[swapchainIndex], myFilteringRenderPass[uint32_t(filterDim)], swapchainIndex, {0,0,res,res});

	vkCmdBindPipeline(myCmdBuffers[swapchainIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, myFilteringPipeline[uint32_t(filterDim)].pipeline);

	// DESCRIPTORS
	theirImageHandler.BindSamplers(myCmdBuffers[swapchainIndex], myFilteringPipeline[uint32_t(filterDim)].layout, 0);
	theirImageHandler.BindImages(swapchainIndex, myCmdBuffers[swapchainIndex], myFilteringPipeline[uint32_t(filterDim)].layout, 1);

	// DRAW
	{
		vkCmdDraw(myCmdBuffers[swapchainIndex], 6, 1, uint32_t(id), mip);
	}

	vkCmdEndRenderPass(myCmdBuffers[swapchainIndex]);
}

void
CubeFilterer::TransferFilteredCubeMip(
	VkImageView		dstImgView,
	uint32_t		dstMip,
	CubeDimension	cubeDim,
	uint32_t		swapchainIndex)
{
	auto dstImg = theirImageAllocator.GetImage(dstImgView);
	auto srcImg = theirImageAllocator.GetImage(myCube);

	uint32_t res = CubeDimValues[uint32_t(cubeDim)];

	VkImageCopy copy{};

	copy.extent = {res,res,1};

	copy.dstSubresource.baseArrayLayer = 0;
	copy.dstSubresource.layerCount = 6;
	copy.dstSubresource.mipLevel = dstMip;
	copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 6;
	copy.srcSubresource.mipLevel = uint32_t(cubeDim);
	copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	// DST TO TRANSFER
	VkImageMemoryBarrier barrier0{};
	barrier0.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier0.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier0.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier0.image = dstImg;
	barrier0.srcQueueFamilyIndex = myPresentationQueueIndex;
	barrier0.dstQueueFamilyIndex = myPresentationQueueIndex;

	barrier0.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier0.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier0.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier0.subresourceRange.baseArrayLayer = 0;
	barrier0.subresourceRange.layerCount = 6;
	barrier0.subresourceRange.baseMipLevel = dstMip;
	barrier0.subresourceRange.levelCount = 1;

	vkCmdPipelineBarrier(myCmdBuffers[swapchainIndex],
						  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  VK_DEPENDENCY_BY_REGION_BIT,
						  0, nullptr,
						  0, nullptr,
						  1, &barrier0
	);

	vkCmdCopyImage(myCmdBuffers[swapchainIndex],
					srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					dstImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1, &copy);

	// DST TO SHADER READ
	VkImageMemoryBarrier barrier1 = barrier0;
	barrier1.dstAccessMask = barrier0.srcAccessMask;
	barrier1.srcAccessMask = barrier0.dstAccessMask;
	barrier1.oldLayout = barrier0.newLayout;
	barrier1.newLayout = barrier0.oldLayout;

	vkCmdPipelineBarrier(myCmdBuffers[swapchainIndex],
						  VK_PIPELINE_STAGE_TRANSFER_BIT,
						  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						  VK_DEPENDENCY_BY_REGION_BIT,
						  0, nullptr,
						  0, nullptr,
						  1, &barrier1
	);
}