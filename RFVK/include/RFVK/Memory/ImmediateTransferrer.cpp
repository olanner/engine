#include "pch.h"
#include "ImmediateTransferrer.h"
#include "RFVK/VulkanFramework.h"

ImmediateTransferrer::ImmediateTransferrer(
	VulkanFramework& vulkanFramework)
	: theirVulkanFramework(vulkanFramework)
{
	VkResult failure;
	std::tie(failure, myImmediateTransQueue, myTransQueueIndex) = theirVulkanFramework.RequestQueue(VK_QUEUE_TRANSFER_BIT);
	assert(!failure && "transfer queue request failed");
	for (uint32_t i = 0; i < MaxNumImmediateTransfers; ++i)
	{
		SentCommandBuffer buffer{};
		std::tie(failure, buffer.cmdBuffer) = theirVulkanFramework.RequestCommandBuffer(myTransQueueIndex);
		assert(!failure && "command buffer request failed");

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.pNext = nullptr;
		fenceInfo.flags = NULL;

		failure = vkCreateFence(theirVulkanFramework.GetDevice(), &fenceInfo, nullptr, &buffer.fence);
		assert(!failure && "failed creating fence");

		myImmediateTransCmdBuffers.push(buffer);
	}
}

ImmediateTransferrer::~ImmediateTransferrer()
{
	neat::static_vector<SentCommandBuffer, MaxNumImmediateTransfers> toDestroy;
	SentCommandBuffer sentCmdBuffer{};
	while (bool success = myImmediateTransCmdBuffers.try_pop(sentCmdBuffer) || toDestroy.size() < MaxNumImmediateTransfers)
	{
		if (success)
		{
			toDestroy.emplace_back(sentCmdBuffer);
		}
	}
	for (auto& [cmdBuffer, fence] : toDestroy)
	{
		vkDestroyFence(theirVulkanFramework.GetDevice(), fence, nullptr);
	}
}

VkResult
ImmediateTransferrer::DoImmediateTransfer(std::function<void(VkCommandBuffer)>&& cmdBatch)
{
	SentCommandBuffer toSend{};
	while (!myImmediateTransCmdBuffers.try_pop(toSend))
	{
	}

	auto& [cmdBuffer, fence] = toSend;

	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	beginInfo.pInheritanceInfo = nullptr;

	auto resultBegin = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
	cmdBatch(cmdBuffer);
	auto resultEnd = vkEndCommandBuffer(cmdBuffer);

	VkSubmitInfo subInfo{};
	subInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	subInfo.pNext = nullptr;

	subInfo.commandBufferCount = 1;
	subInfo.pCommandBuffers = &cmdBuffer;

	subInfo.signalSemaphoreCount = 0;
	subInfo.waitSemaphoreCount = 0;
	subInfo.pSignalSemaphores = nullptr;
	subInfo.pWaitSemaphores = nullptr;
	subInfo.pWaitDstStageMask = nullptr;

	auto resSubmit =
		vkQueueSubmit(myImmediateTransQueue,
					   1,
					   &subInfo,
					   fence
		);
	if (resSubmit)
	{
		vkResetFences(theirVulkanFramework.GetDevice(), 1, &fence);
		return resSubmit;
	}

	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), fence))
	{
	}

	vkResetFences(theirVulkanFramework.GetDevice(), 1, &fence);

	myImmediateTransCmdBuffers.push(toSend);

	return VK_SUCCESS;
}

QueueFamilyIndex
ImmediateTransferrer::GetOwner() const
{
	return myTransQueueIndex;
}
