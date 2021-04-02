#pragma once

class AllocatorBase
{
public:
										AllocatorBase(
											class VulkanFramework&		vulkanFramework,
											class ImmediateTransferrer& immediateTransferrer,
											QueueFamilyIndex			transferFamilyIndex);
										~AllocatorBase();

	std::tuple<VkCommandBuffer, VkFence> AcquireNextTransferBuffer();

protected:
	bool								IsExclusive(
											const QueueFamilyIndex* firstOwner,
											uint32_t numOwners);
	std::tuple<VkMemoryRequirements, MemTypeIndex>
										GetMemReq(
											VkBuffer				buffer, 
											VkMemoryPropertyFlags	memPropFlags);
	std::tuple<VkMemoryRequirements, MemTypeIndex>
										GetMemReq(
											VkImage					image, 
											VkMemoryPropertyFlags	memPropFlags);


	[[nodiscard]] std::tuple<VkResult, VkBuffer, VkDeviceMemory>
										CreateStagingBuffer(
											const void*				data,
											size_t					size,
											const QueueFamilyIndex*	firstOwner,
											uint32_t				numOwners,
											bool					forImmediateUse = false);

	VulkanFramework&					theirVulkanFramework;
	ImmediateTransferrer&				theirImmediateTransferrer;
	VkPhysicalDeviceMemoryProperties	myPhysicalDeviceMemProperties{};
	std::unordered_map<VkMemoryPropertyFlags, std::vector<MemTypeIndex>>
										myMemoryTypes;

	std::mutex							myTransferMutex;
	std::array<neat::static_vector<std::pair<VkBuffer, VkDeviceMemory>, MaxNumTransfers>, 2>
										myStagingBuffers;
	std::array<VkCommandBuffer, 2>		myTransferBuffers;
	std::array<VkFence, 2>				myTransferFences;
	uint32_t							myTransferIndex = 0;

};