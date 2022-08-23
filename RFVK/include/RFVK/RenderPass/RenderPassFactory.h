#pragma once
#include "RenderPass.h"
#include "RenderPassBuilder.h"

enum class RenderPassRequestFlags
{
	BuildFramebuffer = 1 << 0,
	UseDepthAttachment = 1 << 1,
	UseStencil = 1 << 2,

};

class RenderPassFactory
{
public:
												RenderPassFactory(
													class VulkanFramework&	vulkanFramework,
													class ImageAllocator&	imageAllocator,
													QueueFamilyIndex*		firstOwner,
													uint32_t				numOwners);
												~RenderPassFactory();

	RenderPassBuilder							GetConstructor();
	VkResult									CreateInputDescriptors(
													RenderPass&			renderPass,
													RenderPassBuilder&	constructor,
													uint32_t			subpassIndex) const;
	void										RegisterRenderPass(const RenderPass& renderPass);

	std::array<VkImageView, NumSwapchainImages> GetIntermediateAttachmentViews() const;
	VkAttachmentDescription						GetIntermediateAttachmentDesc() const;


private:
	VulkanFramework&							theirVulkanFramework;
	ImageAllocator&								theirImageAllocator;

	std::vector<QueueFamilyIndex>				myOwners;

	VkDescriptorPool							myInputAttachmentPool;

	neat::static_vector<RenderPass, 64>			myRenderPasses;

	std::array<VkImageView, NumSwapchainImages>	myIntermediateViews;
	VkAttachmentDescription						myIntermediateAttachmentDescription;

};