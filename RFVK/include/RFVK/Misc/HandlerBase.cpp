#include "pch.h"
#include "HandlerBase.h"
#include "RFVK/VulkanFramework.h"

HandlerBase::HandlerBase(
	VulkanFramework& vulkanFramework)
	: theirVulkanFramework(vulkanFramework)
{
	myFailedWrites.reserve(myMaxUpdates);
}

void HandlerBase::UpdateDescriptors(int swapchainIndex, VkFence fence)
{
	if (myQueuedDescriptorWrites[swapchainIndex].empty())
	{
		return;
	}
	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), fence))
	{
	}

	QueuedDescriptorWrite queuedWrite;
	int count = 0;
	while (myQueuedDescriptorWrites[swapchainIndex].try_pop(queuedWrite)
		&& count++ < myMaxUpdates)
	{
		if (queuedWrite.waitEvent && *queuedWrite.waitEvent)
		{
			if (vkGetEventStatus(theirVulkanFramework.GetDevice(), *queuedWrite.waitEvent) != VK_EVENT_SET)
			{
				myFailedWrites.emplace_back(queuedWrite);
				continue;
			}
		}
		if (queuedWrite.doneSignal)
		{
			queuedWrite.doneSignal->release();
		}
		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &queuedWrite.write, 0, nullptr);
	}
	
	for (auto& again : myFailedWrites)
	{
		myQueuedDescriptorWrites[swapchainIndex].push(again);
	}
	myFailedWrites.clear();
}

void
HandlerBase::QueueDescriptorUpdate(
	int							swapchainIndex,
	std::shared_ptr<VkEvent>	waitEvent,
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>
								doneSignal,
	VkWriteDescriptorSet		write)
{
	QueuedDescriptorWrite queued;
	queued.waitEvent	= std::move(waitEvent);
	queued.write		= write;
	queued.doneSignal	= std::move(doneSignal);
	myQueuedDescriptorWrites[swapchainIndex].push(queued);
}
