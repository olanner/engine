
#pragma once
#include "RFVK/Misc/HandlerBase.h"

struct Image
{
	VkDescriptorImageInfo	info;
	VkImageView				view;
	Vec2f					dim;
	Vec2f					scale;
	uint32_t				layers;
};

struct ImageCube
{
	VkDescriptorImageInfo	info;
	VkImageView				view;
	float					dim;
};

class ImageHandler : public HandlerBase
{

public:
													ImageHandler(
														class VulkanFramework&	vulkanFramework,
														class ImageAllocator&	imageAllocator,
														QueueFamilyIndices		familyIndices);
													~ImageHandler();

	ImageID											AddImage2D();
	void											RemoveImage2D(ImageID imageID);
	void											UnloadImage2D(ImageID imageID);
	void											LoadImage2D(
														ImageID						imageID,
														AllocationSubmissionID		allocSubID,
														const std::string&			path);
	void											LoadImage2D(
														ImageID imageID,
														AllocationSubmissionID		allocSubID,
														std::vector<uint8_t>&&		pixelData,
														Vec2f						dimension,
														VkFormat					format = VK_FORMAT_R8G8B8A8_UNORM,
														uint32_t					byteDepth = 4);
	void											LoadImage2DTiled(
														ImageID imageID,
														AllocationSubmissionID		allocSubID,
														const std::string&			path,
														uint32_t					rows,
														uint32_t					cols);
	CubeID											AddImageCube();
	shared_semaphore<NumSwapchainImages>			LoadImageCube(
														CubeID						cubeID,
														AllocationSubmissionID		allocSubID,
														const std::string&			path);
	VkResult										LoadStorageImage(
														uint32_t	index,
														VkFormat	format,
														uint32_t	width,
														uint32_t	height);

	void											BindSamplers(
														VkCommandBuffer		commandBuffer,
														VkPipelineLayout	pipelineLayout,
														uint32_t			setIndex,
														VkPipelineBindPoint	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);
	void											BindImages(
														int					swapchainIndex,
														VkCommandBuffer		commandBuffer,
														VkPipelineLayout	pipelineLayout,
														uint32_t			setIndex,
														VkPipelineBindPoint	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

	_nodiscard VkDescriptorSetLayout				GetImageSetLayout() const;
	_nodiscard VkDescriptorSetLayout				GetSamplerSetLayout() const;
	ImageAllocator&									GetImageAllocator() const;

	Image											operator[](ImageID id);
	ImageCube										operator[](CubeID id);

private:
	std::tuple<std::vector<uint8_t>, int, int, int>	ReadImage(const std::string& path);
	void											CreateSampler(
														VkFilter				filter, 
														VkSamplerAddressMode	samplerMode,
														float					anisotropy);
	
	ImageAllocator&									theirImageAllocator;
	std::vector<QueueFamilyIndex>					myOwners;
	std::unordered_map<neat::ImageSwizzle, VkFormat>
													myImageSwizzleToFormat;
	
	VkDescriptorImageInfo							myDefaultImage2DInfo = {};
	VkWriteDescriptorSet							myDefaultImage2DWrite = {};
	VkDescriptorPool								myDescriptorPool = nullptr;
	VkDescriptorSetLayout							myImageSetLayout = nullptr;
	std::array<VkDescriptorSet, NumSwapchainImages>	myImageSets = {};

	neat::static_vector<VkSampler, MaxNumSamplers>	mySamplers;
	VkDescriptorSetLayout							mySamplerSetLayout = nullptr;
	VkDescriptorSet									mySamplerSet = nullptr;

	std::array<Image, MaxNumImages>					myImages2D = {};
	IDKeeper<ImageID>								myImageIDKeeper;

	std::array<ImageCube, MaxNumImagesCube>			myImagesCube = {};
	IDKeeper<CubeID>								myCubeIDKeeper;


	


};
