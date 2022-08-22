#pragma once
#include "Reflex/VK/Image/CubeFilterer.h"
#include "Reflex/VK/Image/CubeFilterer.h"

struct QueuedDescriptorWrite
{
	std::shared_ptr<VkEvent>										waitEvent;
	VkWriteDescriptorSet											write = {};
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	doneSignal;
};

class HandlerBase
{
public:
								HandlerBase(class VulkanFramework& vulkanFramework);
	
	void						UpdateDescriptors(
									int							swapchainIndex,
									VkFence						fence);
	void
								QueueDescriptorUpdate(
									int							swapchainIndex,
									std::shared_ptr<VkEvent>	waitEvent, 
									std::shared_ptr<std::counting_semaphore<NumSwapchainImages>>	
																doneSignal,
									VkWriteDescriptorSet		write);
protected:
	VulkanFramework&			theirVulkanFramework;

private:
	std::array<conc_queue<QueuedDescriptorWrite>, NumSwapchainImages>
								myQueuedDescriptorWrites;
	
	const int					myMaxUpdates = 128;
	std::vector<QueuedDescriptorWrite>	myFailedWrites;
	
};