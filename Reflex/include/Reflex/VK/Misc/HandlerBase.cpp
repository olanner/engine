#include "pch.h"
#include "HandlerBase.h"
#include "Reflex/VK/VulkanFramework.h"

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

	QueuedWrite queuedWrite;
	int count = 0;
	while (myQueuedDescriptorWrites[swapchainIndex].try_pop(queuedWrite)
		&& count++ < myMaxUpdates)
	{
		if (*queuedWrite.event)
		{
			if (vkGetEventStatus(theirVulkanFramework.GetDevice(), *queuedWrite.event) != VK_EVENT_SET)
			{
				myFailedWrites.emplace_back(queuedWrite);
				continue;
			}
		}
		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &queuedWrite.write, 0, nullptr);
	}
	
	for (auto& again : myFailedWrites)
	{
		myQueuedDescriptorWrites[swapchainIndex].push(again);
	}
	myFailedWrites.clear();
}
