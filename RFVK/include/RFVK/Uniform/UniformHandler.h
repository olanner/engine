
#pragma once

class UniformHandler
{
public:
												UniformHandler(
													class VulkanFramework&	vulkanFramework,
													class BufferAllocator&	bufferAllocator,
													QueueFamilyIndices		familyIndices);
												~UniformHandler();

	VkResult									Init();

	UniformID									RequestUniformBuffer(
													const void* startData,
													size_t size);
	void										UpdateUniformData(
													UniformID	id, 
													const void* data);

	std::tuple<VkDescriptorSetLayout, VkDescriptorSet>
												operator[](UniformID id);

	void										BindUniform(
													UniformID			id,
													VkCommandBuffer		cmdBuffer,
													VkPipelineLayout	layout,
													uint32_t			setSlot,
													VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

private:
	VulkanFramework&							theirVulkanFramework;
	BufferAllocator&							theirBufferAllocator;

	std::vector<QueueFamilyIndex>				myOwners;

	std::array<VkBuffer, MaxNumUniforms>		myUniforms;
	concurrency::concurrent_priority_queue<UniformID, std::greater<>>
												myFreeIDs;

	VkDescriptorPool							myDescriptorPool;
	std::unordered_map<VkBuffer, VkDescriptorSetLayout>
												myUniformLayouts;
	std::unordered_map<VkBuffer, VkDescriptorSet>
												myUniformSets;


};