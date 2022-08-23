#include "pch.h"
#include "RenderPassBuilder.h"



#include "RenderPassFactory.h"
#include "RFVK/VulkanFramework.h"
#include "RFVK/Memory/ImageAllocator.h"

SubpassBuilder::SubpassBuilder()
: myColorAttachments{}
, myDepthAttachment{}
, myInputAttachments{}
, myDescription{}
{
}

SubpassBuilder& 
SubpassBuilder::SetColorAttachments(std::vector<uint32_t>&& attachmentIndices)
{
	if (attachmentIndices.size() > MaxNumAttachments)
	{
		LOG("failed setting color attachments for subpass, too many attachments, max is", MaxNumAttachments);
		return *this;
	}

	myNumColorAttachments = attachmentIndices.size();

	for (size_t index = 0; index < myNumColorAttachments; index++)
	{
		VkAttachmentReference ref;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		ref.attachment = attachmentIndices[index];
		myColorAttachments[index] = ref;
	}

	myDescription.colorAttachmentCount = myNumColorAttachments;
	myDescription.pColorAttachments = myColorAttachments.data();

	return *this;
}

SubpassBuilder& 
SubpassBuilder::SetInputAttachments(std::vector<uint32_t>&& attachmentIndices)
{
	if (attachmentIndices.size() > MaxNumAttachments)
	{
		LOG("failed setting input attachments for subpass, too many attachments, max is", MaxNumAttachments);
		return *this;
	}

	myNumInputAttachments = attachmentIndices.size();

	for (size_t index = 0; index < myNumInputAttachments; index++)
	{
		VkAttachmentReference ref;
		ref.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		ref.attachment = attachmentIndices[index];
		myInputAttachments[index] = ref;
	}

	myDescription.inputAttachmentCount = myNumInputAttachments;
	myDescription.pInputAttachments = myInputAttachments.data();

	return *this;
}

SubpassBuilder& 
SubpassBuilder::SetDepthAttachment(uint32_t attachmentIndex)
{
	VkAttachmentReference ref;
	ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	ref.attachment = attachmentIndex;
	myDepthAttachment = ref;

	myDescription.pDepthStencilAttachment = &myDepthAttachment;

	return *this;
}

RenderPassBuilder::RenderPassBuilder(
	VulkanFramework&	vulkanFramework,
	ImageAllocator&		imageAllocator,
	RenderPassFactory&	renderPassFactory,
	QueueFamilyIndex*	firstOwner,
	uint32_t			numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirImageAllocator(imageAllocator)
	, theirRenderPassFactory(renderPassFactory)
	, myOwners{}

{
	myOwners.resize(numOwners);
	for (uint32_t familyIndex = 0; familyIndex < numOwners; ++familyIndex)
	{
		myOwners[familyIndex] = firstOwner[familyIndex];
	}

	mySubpasses.reserve(MaxNumSubpasses);
}

void
RenderPassBuilder::DefineAttachments(
	std::vector<VkFormat>&& attachmentFormats, 
	Vec2f					attachmentRes)
{
	if (attachmentFormats.size() > MaxNumAttachments)
	{
		LOG("too many attachments for render pass defined,", attachmentFormats.size(), "max is", MaxNumAttachments);
		return;
	}

	myNumAttachments = attachmentFormats.size();
	myAttachmentRes = attachmentRes;

	myAttachmentDebugNames.resize(myNumAttachments);

	memcpy(myAttachmentFormats.data(), attachmentFormats.data(), attachmentFormats.size() * sizeof VkFormat);


	for (uint32_t attIndex = 0; attIndex < myNumAttachments; attIndex++)
	{
		if (attachmentFormats[attIndex] == IntermediateAttachment)
		{
			myAttachmentDescriptions[attIndex] = theirRenderPassFactory.GetIntermediateAttachmentDesc();
			continue;
		}
		if (attachmentFormats[attIndex] == SwapchainAttachment)
		{
			myAttachmentDescriptions[attIndex] = theirVulkanFramework.GetSwapchainAttachmentDesc();
			continue;
		}


		bool isDepth = attachmentFormats[attIndex] == VK_FORMAT_D32_SFLOAT || attachmentFormats[attIndex] == VK_FORMAT_D24_UNORM_S8_UINT;

		myAttachmentDescriptions[attIndex].flags = NULL;

		myAttachmentDescriptions[attIndex].format = attachmentFormats[attIndex];
		myAttachmentDescriptions[attIndex].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		myAttachmentDescriptions[attIndex].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		myAttachmentDescriptions[attIndex].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		myAttachmentDescriptions[attIndex].finalLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		myAttachmentDescriptions[attIndex].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		myAttachmentDescriptions[attIndex].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

		myAttachmentDescriptions[attIndex].samples = VK_SAMPLE_COUNT_1_BIT;

		myAttachmentDebugNames[attIndex] = std::string(isDepth ? "depth_" : "col_") + "no." + std::to_string(attIndex);

		myAttachmentClearValues[attIndex].color = {};
		myAttachmentClearValues[attIndex].depthStencil = {1,0};
	}
}

void
RenderPassBuilder::SetAttachmentNames(std::vector<std::string>&& names)
{
	if (names.size() != myNumAttachments)
	{
		return;
	}
	myAttachmentDebugNames = names;
}

void
RenderPassBuilder::SetClearValues(std::vector<VkClearValue>&& clearValues)
{
	if (clearValues.size() > myNumAttachments)
	{
		return;
	}
	for (size_t attachmentIndex = 0; attachmentIndex < clearValues.size(); ++attachmentIndex)
	{
		myAttachmentClearValues[attachmentIndex] = clearValues[attachmentIndex];
	}
}

void
RenderPassBuilder::SetImageViews(
	uint32_t	attachmentIndex, 
	VkImageView view)
{
	myAttachmentViews[attachmentIndex].fill(view);
}

void
RenderPassBuilder::SetImageViews(
	uint32_t										attachmentIndex, 
	std::array<VkImageView, NumSwapchainImages>&&	views)
{
	myAttachmentViews[attachmentIndex] = views;
}

SubpassBuilder& 
RenderPassBuilder::AddSubpass()
{
	return mySubpasses.emplace_back(SubpassBuilder());
}

void
RenderPassBuilder::AddSubpassDependency(VkSubpassDependency&& dependency)
{
	mySubpassDependencies.emplace_back(dependency);
}

VkAttachmentDescription& 
RenderPassBuilder::EditAttachmentDescription(uint32_t attachmentIndex)
{
	return myAttachmentDescriptions[attachmentIndex];
}

RenderPass
RenderPassBuilder::Build()
{
	RenderPass retPass{};
	retPass.numSubpasses = mySubpasses.size();

	retPass.numAttachments = myNumAttachments;
	retPass.clearValues = myAttachmentClearValues;

	std::vector<VkSubpassDescription> subpassDescriptions;
	for (auto& subpass : mySubpasses)
	{
		subpassDescriptions.emplace_back(subpass.myDescription);
	}

	// RENDER PASS
	VkRenderPassCreateInfo rpInfo{};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.pNext = nullptr;
	rpInfo.flags = NULL;

	rpInfo.attachmentCount = myNumAttachments;
	rpInfo.pAttachments = myAttachmentDescriptions.data();

	rpInfo.pSubpasses = subpassDescriptions.data();
	rpInfo.subpassCount = mySubpasses.size();

	rpInfo.pDependencies = mySubpassDependencies.data();
	rpInfo.dependencyCount = mySubpassDependencies.size();

	auto result = vkCreateRenderPass(theirVulkanFramework.GetDevice(), &rpInfo, nullptr, &retPass.renderPass);
	if (result)
	{
		LOG("failed creating render pass");
		return {};
	}
	// FRAMEBUFFER
	{
		result = CreateAttachmentImages();
		if (result)
		{
			LOG("failed creating render pass");
			return retPass;
		}
		result = BuildFramebuffer(retPass);
		if (result)
		{
			LOG("failed creating render pass");
			return retPass;
		}
	}

	// INPUT ATTACHMENTS
	for (uint32_t subpassIndex = 0; subpassIndex < mySubpasses.size(); ++subpassIndex)
	{
		if (mySubpasses[subpassIndex].myNumInputAttachments > 0)
		{
			theirRenderPassFactory.CreateInputDescriptors(retPass, *this, subpassIndex);
		}
	}

	theirRenderPassFactory.RegisterRenderPass(retPass);
	return retPass;
}

VkResult
RenderPassBuilder::CreateAttachmentImages()
{
	for (uint32_t attIndex = 0; attIndex < myNumAttachments; ++attIndex)
	{
		if (myAttachmentViews[attIndex][0])
		{
			continue;
		}
		if (myAttachmentFormats[attIndex] == IntermediateAttachment)
		{
			myAttachmentViews[attIndex] = theirRenderPassFactory.GetIntermediateAttachmentViews();
			continue;
		}
		if (myAttachmentFormats[attIndex] == SwapchainAttachment)
		{
			myAttachmentViews[attIndex] = theirVulkanFramework.GetSwapchainImageViews();
			continue;
		}
		VkImageLayout layout{};
		VkImageUsageFlags usage{};
		VkPipelineStageFlags stage{};


		bool isDepth = myAttachmentFormats[attIndex] == VK_FORMAT_D32_SFLOAT || myAttachmentFormats[attIndex] == VK_FORMAT_D24_UNORM_S8_UINT;
		if (isDepth)
		{
			layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
		{
			layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
			stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		}


		{
			auto allocSub = theirImageAllocator.Start();
			ImageRequestInfo requestInfo;
			requestInfo.width = myAttachmentRes.x;
			requestInfo.height = myAttachmentRes.y;
			requestInfo.owners = myOwners;
			requestInfo.format = myAttachmentFormats[attIndex];
			requestInfo.layout = layout;
			requestInfo.usage = usage;
			requestInfo.targetPipelineStage = stage;
			
			auto [result, view] = theirImageAllocator.RequestImage2D(
				allocSub, 
				nullptr,
	            0,
				requestInfo);
			if (result)
			{
				LOG("render pass creation error: failed creating attachment image,", attIndex, "for framebuffer");
				return result;
			}
			else
			{
				theirImageAllocator.SetDebugName(view, myAttachmentDebugNames[attIndex].c_str());
				myAttachmentViews[attIndex].fill(view);
			}
			theirImageAllocator.Queue(std::move(allocSub));
		}


	}
	return VK_SUCCESS;
}

VkResult
RenderPassBuilder::BuildFramebuffer(RenderPass& renderPass)
{
	for (uint32_t scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;

		framebufferInfo.renderPass = renderPass.renderPass;
		framebufferInfo.layers = 1;

		framebufferInfo.width = myAttachmentRes.x;
		framebufferInfo.height = myAttachmentRes.y;

		std::vector<VkImageView> attViews(myNumAttachments, nullptr);
		for (uint32_t attIndex = 0; attIndex < myNumAttachments; ++attIndex)
		{
			attViews[attIndex] = myAttachmentViews[attIndex][scIndex];
		}

		framebufferInfo.pAttachments = attViews.data();
		framebufferInfo.attachmentCount = myNumAttachments;

		auto result = vkCreateFramebuffer(theirVulkanFramework.GetDevice(), &framebufferInfo, nullptr, &renderPass.frameBuffers[scIndex]);
		if (result)
		{
			LOG("failed creating framebuffer for renderpass");
			return result;
		}
	}

	return VK_SUCCESS;
}