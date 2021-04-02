#pragma once
#include "RenderPass.h"

class SubpassBuilder
{
	friend class RenderPassBuilder;
	friend class RenderPassFactory;

public:
	SubpassBuilder&			SetColorAttachments(std::vector<uint32_t>&& attachmentIndices);
	SubpassBuilder&			SetInputAttachments(std::vector<uint32_t>&& attachmentIndices);
	SubpassBuilder&			SetDepthAttachment(uint32_t attachmentIndex);

private:
							SubpassBuilder();

	uint32_t				myNumColorAttachments = 0;
	std::array<VkAttachmentReference, MaxNumAttachments>
							myColorAttachments;
	VkAttachmentReference	myDepthAttachment;

	uint32_t				myNumInputAttachments = 0;
	std::array<VkAttachmentReference, MaxNumAttachments>
							myInputAttachments;

	VkSubpassDescription	myDescription;

};

constexpr VkFormat IntermediateAttachment = VK_FORMAT_UNDEFINED;
constexpr VkFormat SwapchainAttachment = VK_FORMAT_RANGE_SIZE;
class RenderPassBuilder
{
	friend class RenderPassFactory;

public:
	void										DefineAttachments(
													std::vector<VkFormat>&& attachmentFormats,
													Vec2f					attachmentRes);

	SubpassBuilder&								AddSubpass();
	void										AddSubpassDependency(VkSubpassDependency&& dependency);

	void										SetAttachmentNames(std::vector<std::string>&& names);
	void										SetClearValues(std::vector<VkClearValue>&& clearValues);
	void										SetImageViews(
													uint32_t	attachmentIndex, 
													VkImageView view);
	void										SetImageViews(
													uint32_t										attachmentIndex, 
													std::array<VkImageView, NumSwapchainImages>&&	views);

	VkAttachmentDescription&					EditAttachmentDescription(uint32_t attachmentIndex);

	RenderPass									Build();


private:
												RenderPassBuilder(
													class VulkanFramework&		vulkanFramework,
													class ImageAllocator&		imageAllocator,
													class RenderPassFactory&	renderPassFactory,
													QueueFamilyIndex*			firstOwner,
													uint32_t					numOwners);


	VkResult									CreateAttachmentImages();
	VkResult									BuildFramebuffer(RenderPass& renderPass);

	VulkanFramework&							theirVulkanFramework;
	ImageAllocator&								theirImageAllocator;
	RenderPassFactory&							theirRenderPassFactory;

	neat::static_vector<QueueFamilyIndex, 8>	myOwners;

	Vec2f										myAttachmentRes{};

	uint32_t									myNumAttachments = 0;
	std::array<VkFormat, MaxNumAttachments>		myAttachmentFormats;
	std::array<VkAttachmentDescription, MaxNumAttachments>
												myAttachmentDescriptions;
	std::array<std::array<VkImageView, NumSwapchainImages>, MaxNumAttachments>
												myAttachmentViews;
	std::array<VkClearValue, MaxNumAttachments>	myAttachmentClearValues;

	std::vector<std::string>					myAttachmentDebugNames;

	std::vector<SubpassBuilder>					mySubpasses;
	std::vector<VkSubpassDependency>			mySubpassDependencies;

};

