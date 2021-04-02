
#pragma once

struct SentCommandBuffer
{
	VkCommandBuffer	cmdBuffer;
	VkFence			fence;
};

class ImmediateTransferrer
{
public:
	ImmediateTransferrer(class VulkanFramework& vulkanFramework);
	~ImmediateTransferrer();

	VkResult			DoImmediateTransfer(std::function<void(VkCommandBuffer)>&& cmdBatch);

	QueueFamilyIndex	GetOwner() const;

private:
	VulkanFramework&	theirVulkanFramework;

	QueueFamilyIndex	myTransQueueIndex;
	VkQueue				myImmediateTransQueue;

	concurrency::concurrent_queue<SentCommandBuffer>
						myImmediateTransCmdBuffers;

};