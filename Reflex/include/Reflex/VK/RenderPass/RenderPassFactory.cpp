#include "pch.h"
#include "RenderPassFactory.h"

#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Debug/DebugUtils.h"
#include "Reflex/VK/Memory/ImageAllocator.h"

RenderPassFactory::RenderPassFactory(
	VulkanFramework&	vulkanFramework,
	ImageAllocator&		imageAllocator,
	QueueFamilyIndex*	firstOwner,
	uint32_t			numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirImageAllocator(imageAllocator)
	, myIntermediateAttachmentDescription{}
{
	for (uint32_t ownerIndex = 0; ownerIndex < numOwners; ++ownerIndex)
	{
		myOwners.emplace_back(firstOwner[ownerIndex]);
	}

	// INTERMEDIATE ATTACHMENT
	auto allocSub = theirImageAllocator.Start();
	
	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	VkResult result;
	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		ImageRequestInfo requestInfo;
		requestInfo.width = w;
		requestInfo.height = h;
		requestInfo.owners = myOwners;
		requestInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		requestInfo.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		requestInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		requestInfo.targetPipelineStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		std::tie(result, myIntermediateViews[scIndex]) = theirImageAllocator.RequestImage2D(
			allocSub, 
			nullptr,
			0,
			requestInfo);
		assert(!result && "failed creating intermediate image");
	}

	theirImageAllocator.Queue(std::move(allocSub));

	myIntermediateAttachmentDescription.format = VK_FORMAT_R8G8B8A8_UNORM;
	myIntermediateAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	myIntermediateAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	myIntermediateAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	myIntermediateAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	myIntermediateAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	myIntermediateAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	myIntermediateAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;

	// INPUT ATTACHMENT POOL
	VkDescriptorPoolSize inputSz{};
	inputSz.descriptorCount = 128;
	inputSz.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

	poolInfo.maxSets = 64;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &inputSz;

	result = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myInputAttachmentPool);
	assert(!result && "failed creating input attachment descriptor pool");

}

RenderPassFactory::~RenderPassFactory()
{
	for (auto& renderPass : myRenderPasses)
	{
		DestroyRenderPass(renderPass, theirVulkanFramework.GetDevice());
	}
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myInputAttachmentPool, nullptr);
}

RenderPassBuilder
RenderPassFactory::GetConstructor()
{
	return RenderPassBuilder(theirVulkanFramework, theirImageAllocator, *this, myOwners.data(), myOwners.size());
}

void
RenderPassFactory::RegisterRenderPass(const RenderPass& renderPass)
{
	bool success = myRenderPasses.emplace_back(renderPass);
	if (!success)
	{
		LOG("failed registering render pass, no more slots left");
	}
}

std::array<VkImageView, NumSwapchainImages>
RenderPassFactory::GetIntermediateAttachmentViews() const
{
	return myIntermediateViews;
}

VkAttachmentDescription
RenderPassFactory::GetIntermediateAttachmentDesc() const
{
	return myIntermediateAttachmentDescription;
}

VkResult
RenderPassFactory::CreateInputDescriptors(
	RenderPass&			renderPass,
	RenderPassBuilder&	constructor,
	uint32_t			subpassIndex) const
{
	uint32_t numInputAttachments = constructor.mySubpasses[subpassIndex].myNumInputAttachments;
	auto inputAttachmentRefs = constructor.mySubpasses[subpassIndex].myInputAttachments;
	auto& subpass = renderPass.subpasses[subpassIndex];

	// LAYOUT
	VkDescriptorSetLayoutBinding binding{};
	binding.descriptorCount = numInputAttachments;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	binding.binding = 0;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	auto result = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &subpass.inputAttachmentLayout);
	if (result)
	{
		LOG("failed creating descriptor set layout for input attachments");
		return result;
	}


	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		// SET
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

		allocInfo.pSetLayouts = &subpass.inputAttachmentLayout;
		allocInfo.descriptorSetCount = 1;

		allocInfo.descriptorPool = myInputAttachmentPool;

		result = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &subpass.inputAttachmentSets[scIndex]);
		if (result)
		{
			LOG("failed allocating descriptor set for input attachments");
			return result;
		}

		// WRITE
		neat::static_vector<VkDescriptorImageInfo, MaxNumAttachments> inputImgInfos;
		for (uint32_t inputIndex = 0; inputIndex < numInputAttachments; ++inputIndex)
		{
			uint32_t attachmentIndex = inputAttachmentRefs[inputIndex].attachment;

			VkDescriptorImageInfo imgInfo{};
			imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imgInfo.imageView = constructor.myAttachmentViews[attachmentIndex][scIndex];

			inputImgInfos.emplace_back(imgInfo);
		}

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

		write.dstSet = subpass.inputAttachmentSets[scIndex];
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		write.descriptorCount = numInputAttachments;
		write.dstArrayElement = 0;

		write.pImageInfo = inputImgInfos.data();

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	}

	return VK_SUCCESS;
}