
#pragma once

class ImageHandler
{
public:
												ImageHandler(
													class VulkanFramework&	vulkanFramework,
													class ImageAllocator&	imageAllocator,
													QueueFamilyIndex*		firstOwner,
													uint32_t				numOwners);
												~ImageHandler();

	ImageID										AddImage2D(const char* path);
	ImageID										AddImage2D(
													std::vector<uint8_t>&&	pixelData,
													Vec2f				dimension,
													VkFormat			format = VK_FORMAT_R8G8B8A8_SRGB,
													uint32_t			byteDepth = 4);
	ImageID										AddImage2DTiled(
													const char* path,
													uint32_t	rows,
													uint32_t	cols);
	CubeID										AddImageCube(const char* path);
	VkResult									AddStorageImage(
													uint32_t	index,
													VkFormat	format,
													uint32_t	width,
													uint32_t	height);

	VkDescriptorSetLayout						GetImageSetLayout() const;
	VkDescriptorSet								GetImageSet() const;
	VkDescriptorSetLayout						GetSamplerSetLayout() const;
	VkDescriptorSet								GetSamplerSet() const;

	void										BindSamplers(
													VkCommandBuffer		commandBuffer,
													VkPipelineLayout	pipelineLayout,
													uint32_t			setIndex,
													VkPipelineBindPoint	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
	void										BindImages(
													VkCommandBuffer		commandBuffer,
													VkPipelineLayout	pipelineLayout,
													uint32_t			setIndex,
													VkPipelineBindPoint	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

	VkImageView									operator[](ImageID id);
	VkImageView									operator[](CubeID id);

	Vec2f										GetImage2DDimension(ImageID id);
	float										GetImageCubeDimension(CubeID id);
	ImageAllocator&								GetImageAllocator() const;

private:
	VulkanFramework&							theirVulkanFramework;
	ImageAllocator&								theirImageAllocator;

	VkDescriptorPool							myDescriptorPool;
	VkDescriptorSet								myImageSet;
	VkDescriptorSetLayout						myImageSetLayout;

	VkSampler									mySampler;
	VkDescriptorSet								mySamplerSet;
	VkDescriptorSetLayout						mySamplerSetLayout;

	std::array<Vec2f, MaxNumImages>				myImages2DDims;
	std::array<VkImageView, MaxNumImages>		myImages2D;
	IDKeeper<ImageID>							myImageIDKeeper;

	std::array<float, MaxNumImagesCube>			myImagesCubeDims;
	std::array<VkImageView, MaxNumImagesCube>	myImagesCube;
	IDKeeper<CubeID>							myCubeIDKeeper;

	neat::static_vector<QueueFamilyIndex, 16>	myOwners;

	std::unordered_map<neat::ImageSwizzle, VkFormat>
												myImageSwizzleToFormat;


};
