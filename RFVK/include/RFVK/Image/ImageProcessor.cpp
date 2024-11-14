#include "pch.h"
#include "ImageProcessor.h"
#include "ImageHandler.h"

#include "RFVK/VulkanFramework.h"
#include "RFVK/Shader/Shader.h"
#include "RFVK/Pipelines/ComputePipelineBuilder.h"
#include "RFVK/Memory/ImageAllocator.h"
#include "RFVK/Debug/DebugUtils.h"

ImageProcessor::ImageProcessor(
	VulkanFramework& vulkanFramework,
	ImageAllocator& imageAllocator,
	ImageHandler& imageHandler,
	QueueFamilyIndices 	familyIndices) :
	theirVulkanFramework(vulkanFramework),
	theirImageAllocator(imageAllocator),
	theirImageHandler(imageHandler)
{
	myWaitStages.fill(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	myComputeQueueIndex = familyIndices[QUEUE_FAMILY_COMPUTE];
	char shaderPaths[][128]
	{
		"Shaders/comp_brdf_lut.comp",
	};
	myImageProcessShader = std::make_shared<Shader>(shaderPaths, _ARRAYSIZE(shaderPaths), theirVulkanFramework);

	auto result = CreateDescriptorSet();
	assert(!result && "failed creating descriptor set");

	ComputePipelineBuilder builder;
	builder.AddShader(myImageProcessShader);
	builder.AddDescriptorSet(theirImageHandler.GetSamplerSetLayout());
	builder.AddDescriptorSet(theirImageHandler.GetImageSetLayout());
	builder.AddDescriptorSet(myDescriptorLayout);
	std::tie(result, myImageProcessPipeline) = builder.Construct(theirVulkanFramework.GetDevice());

	{
		auto allocSubId = theirImageAllocator.Start();

		auto imgID = theirImageHandler.AddImage2D();
		std::vector<uint8_t> pixels;
		pixels.resize(512 * 512 * 4);
		std::fill(pixels.begin(), pixels.end(), uint8_t(255));
		theirImageHandler.LoadImage2D(imgID, allocSubId, std::move(pixels), { 512, 512 }, VK_FORMAT_R16G16_UNORM, 4U);

		ImageRequestInfo imgInfo = {};
		imgInfo.format = VK_FORMAT_R16G16_UNORM;
		imgInfo.targetPipelineStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
		imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
		imgInfo.height = 512;
		imgInfo.width = 512;
		imgInfo.mips = 1;
		imgInfo.owners = { familyIndices[QUEUE_FAMILY_GRAPHICS], familyIndices[QUEUE_FAMILY_COMPUTE] };
		imgInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
		auto [result, imageView] = theirImageAllocator.RequestImage2D(allocSubId, pixels.data(), 512 * 512 * 4, imgInfo);
		assert(!result && "failed creating storage image");
		DebugSetObjectName("img proccess storage img", theirImageAllocator.GetImage(imageView), VK_OBJECT_TYPE_IMAGE_VIEW, theirVulkanFramework.GetDevice());

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.imageView = imageView;

		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		write.dstSet = myDescriptorSet;
		write.descriptorCount = 1;
		write.dstArrayElement = 0;
		write.dstBinding = 0;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}

	// COMMAND BUFFERS
	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		auto [result, cmdBuffer] = theirVulkanFramework.RequestCommandBuffer(familyIndices[QUEUE_FAMILY_GRAPHICS]);
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

neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
ImageProcessor::RecordSubmit(
	uint32_t 														swapchainImageIndex,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores)
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

	auto cmdBuffer = myCmdBuffers[swapchainImageIndex];
	auto resultBegin = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
	if (myHasRun)
	{
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, myImageProcessPipeline.pipeline);
		theirImageHandler.BindSamplers(cmdBuffer, myImageProcessPipeline.layout, 0, VK_PIPELINE_BIND_POINT_COMPUTE);
		theirImageHandler.BindImages(swapchainImageIndex, cmdBuffer, myImageProcessPipeline.layout, 1, VK_PIPELINE_BIND_POINT_COMPUTE);
		vkCmdBindDescriptorSets(
			cmdBuffer,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			myImageProcessPipeline.layout,
			2,
			1,
			&myDescriptorSet,
			0,
			nullptr);
		vkCmdDispatch(cmdBuffer, 512 / 16, 512 / 16, 1);
		myHasRun = true;
	}
	vkEndCommandBuffer(cmdBuffer);

	WorkerSubmission submission;
	submission.desiredQueue = VK_QUEUE_COMPUTE_BIT;
	submission.fence = myCmdBufferFences[swapchainImageIndex];

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pCommandBuffers = &myCmdBuffers[swapchainImageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pWaitDstStageMask = myWaitStages.data();;
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pSignalSemaphores = signalSemaphores.data();
	submitInfo.signalSemaphoreCount = waitSemaphores.size();

	submission.submitInfo = submitInfo;

	return {submission};
}

std::array<VkFence, NumSwapchainImages> ImageProcessor::GetFences()
{
	return {};
}

std::vector<rflx::Features> ImageProcessor::GetImplementedFeatures() const
{
	return { rflx::Features::FEATURE_CORE };
}

VkResult ImageProcessor::CreateDescriptorSet()
{
	assert(!myDescriptorSet && "create descriptors can only be called once");
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	poolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = 1;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &poolSize;

	auto result = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &descPoolInfo, nullptr, &myDescriptorPool);
	if (result)
	{
		return result;
	}

	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorCount = 1;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	result = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &myDescriptorLayout);
	if (result)
	{
		return result;
	}

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &myDescriptorLayout;
	allocInfo.descriptorPool = myDescriptorPool;
	result = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &myDescriptorSet);
	if (result)
	{
		return result;
	}
}
