#include "pch.h"
#include "RenderPass.h"

void
BindSubpassInputs(
	VkCommandBuffer		cmdBuffer,
	VkPipelineLayout	pipelineLayout,
	uint32_t			setIndex,
	Subpass&			subpass,
	uint32_t			swapchainIndex)
{
	vkCmdBindDescriptorSets(cmdBuffer,
							 VK_PIPELINE_BIND_POINT_GRAPHICS,
							 pipelineLayout,
							 setIndex,
							 1,
							 &subpass.inputAttachmentSets[swapchainIndex],
							 0,
							 nullptr);
}

void
BeginRenderPass(
	VkCommandBuffer cmdBuffer,
	RenderPass&		renderPass,
	uint32_t		swapchainIndex,
	Vec4f			renderArea)
{
	VkRenderPassBeginInfo rBeginInfo{};
	rBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rBeginInfo.renderArea = {int32_t(renderArea.x), int32_t(renderArea.y), uint32_t(renderArea.z), uint32_t(renderArea.w)};
	rBeginInfo.renderPass = renderPass.renderPass;
	rBeginInfo.framebuffer = renderPass.frameBuffers[swapchainIndex];
	rBeginInfo.pClearValues = renderPass.clearValues.data();
	rBeginInfo.clearValueCount = renderPass.numAttachments;

	vkCmdBeginRenderPass(cmdBuffer, &rBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void
DestroyRenderPass(
	RenderPass& renderPass, 
	VkDevice	device)
{
	vkDestroyRenderPass(device, renderPass.renderPass, nullptr);
	for (int scIndex = 0; scIndex < NumSwapchainImages; ++scIndex)
	{
		if (renderPass.frameBuffers[scIndex])
		{
			vkDestroyFramebuffer(device, renderPass.frameBuffers[scIndex], nullptr);
		}
	}
	for (uint32_t subpassIndex = 0; subpassIndex < renderPass.numSubpasses; ++subpassIndex)
	{
		if (renderPass.subpasses[subpassIndex].inputAttachmentLayout)
		{
			vkDestroyDescriptorSetLayout(device, renderPass.subpasses[subpassIndex].inputAttachmentLayout, nullptr);
		}
	}
}
