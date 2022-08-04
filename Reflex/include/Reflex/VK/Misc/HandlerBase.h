#pragma once

struct QueuedWrite
{
	std::shared_ptr<VkEvent>	event;
	VkWriteDescriptorSet		write;
};

class HandlerBase
{
public:
								HandlerBase(class VulkanFramework& vulkanFramework);
	
	void						UpdateDescriptors(
									int		swapchainIndex,
									VkFence fence);
	
protected:
	VulkanFramework&			theirVulkanFramework;
	std::array<conc_queue<QueuedWrite>, NumSwapchainImages>
								myQueuedDescriptorWrites;
private:
	const int					myMaxUpdates = 128;
	std::vector<QueuedWrite>	myFailedWrites;
	
};