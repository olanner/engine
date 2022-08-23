
#pragma once

constexpr uint32_t MaxNumAttachments = 16;
constexpr uint32_t MaxNumSubpasses = 8;

struct Subpass
{
	VkDescriptorSetLayout							inputAttachmentLayout;
	std::array<VkDescriptorSet, NumSwapchainImages> inputAttachmentSets;
};

void BindSubpassInputs(
		VkCommandBuffer		cmdBuffer,
		VkPipelineLayout	pipelineLayout,
		uint32_t			setIndex,
		Subpass&			subpass,
		uint32_t			swapchainIndex);

struct RenderPass
{
	VkRenderPass									renderPass;
	std::array<VkFramebuffer, NumSwapchainImages>	frameBuffers;
	uint32_t										numAttachments;
	std::array<VkClearValue, MaxNumAttachments>		clearValues;

	uint32_t										numSubpasses;
	std::array<Subpass, MaxNumSubpasses>			subpasses;

};

void BeginRenderPass(
		VkCommandBuffer cmdBuffer,
		RenderPass&		renderPass,
		uint32_t		swapchainIndex,
		Vec4f			renderArea);

void DestroyRenderPass(
		RenderPass& renderPass, 
		VkDevice	device);