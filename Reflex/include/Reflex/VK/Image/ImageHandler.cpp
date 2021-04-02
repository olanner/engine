#include "pch.h"
#include "neat/Image/DDSReader.h"
#include "ImageHandler.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/ImageAllocator.h"

ImageHandler::ImageHandler(
	VulkanFramework&	vulkanFramework,
	ImageAllocator&		imageAllocator,
	QueueFamilyIndex*	firstOwner,
	uint32_t			numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirImageAllocator(imageAllocator)
	, myDescriptorPool()
	, myImageArraysSet()
	, myImageArraysSetLayout()
	, mySampler()
	, mySamplerSet()
	, mySamplerSetLayout()
	, myImages2D{}
	, myImageIDKeeper(MaxNumImages2D)
	, myImagesCube{}
	, myCubeIDKeeper(MaxNumImagesCube)
	, myOwners{}
{
	for (uint32_t i = 0; i < numOwners; ++i)
	{
		myOwners.emplace_back(firstOwner[i]);
	}

	// POOL
	VkDescriptorPoolSize sampledImageDescriptorSize;
	sampledImageDescriptorSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	sampledImageDescriptorSize.descriptorCount = MaxNumImages2D + MaxNumImagesCube;
	VkDescriptorPoolSize storageImageDescriptorSize;
	storageImageDescriptorSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageImageDescriptorSize.descriptorCount = MaxNumStorageImages;
	VkDescriptorPoolSize samplerDescriptorsSize;
	samplerDescriptorsSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerDescriptorsSize.descriptorCount = MaxNumSamplers;
	VkDescriptorPoolSize image2DArrayDescriptorSize;
	image2DArrayDescriptorSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	image2DArrayDescriptorSize.descriptorCount = MaxNumSamplers;

	std::array<VkDescriptorPoolSize, 3> poolSizes
	{
		sampledImageDescriptorSize,
		storageImageDescriptorSize,
		samplerDescriptorsSize
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = NULL;

	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 2;

	auto resultPool = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!resultPool && "failed creating pool");

	// IMAGE ARRAYS DESCRIPTOR
	//LAYOUT
	VkDescriptorSetLayoutBinding sampImgArrBinding;
	sampImgArrBinding.descriptorCount = MaxNumImages2D;
	sampImgArrBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	sampImgArrBinding.binding = 0;
	sampImgArrBinding.pImmutableSamplers = nullptr;
	sampImgArrBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutBinding storageImgArrBinding;
	storageImgArrBinding.descriptorCount = MaxNumStorageImages;
	storageImgArrBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageImgArrBinding.binding = 1;
	storageImgArrBinding.pImmutableSamplers = nullptr;
	storageImgArrBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutBinding cubeMapArrBinding;
	cubeMapArrBinding.descriptorCount = MaxNumImagesCube;
	cubeMapArrBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	cubeMapArrBinding.binding = 2;
	cubeMapArrBinding.pImmutableSamplers = nullptr;
	cubeMapArrBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutCreateInfo imgArrLayoutInfo;
	imgArrLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	imgArrLayoutInfo.pNext = nullptr;
	imgArrLayoutInfo.flags = NULL;

	VkDescriptorSetLayoutBinding bindings[]
	{
		sampImgArrBinding,
		storageImgArrBinding,
		cubeMapArrBinding,
	};

	imgArrLayoutInfo.bindingCount = ARRAYSIZE(bindings);
	imgArrLayoutInfo.pBindings = bindings;

	auto resultLayout = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &imgArrLayoutInfo, nullptr, &myImageArraysSetLayout);
	assert(!resultLayout && "failed creating image array descriptor set LAYOUT");

	// SET
	VkDescriptorSetAllocateInfo imgArrAllocInfo;
	imgArrAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	imgArrAllocInfo.pNext = nullptr;

	imgArrAllocInfo.descriptorSetCount = 1;
	imgArrAllocInfo.pSetLayouts = &myImageArraysSetLayout;
	imgArrAllocInfo.descriptorPool = myDescriptorPool;

	auto resultAlloc = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &imgArrAllocInfo, &myImageArraysSet);
	assert(!resultAlloc && "failed creating image array descriptor set");

	// SAMPLER
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.pNext = nullptr;
	samplerInfo.flags = NULL;

	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 8.f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = 16.f;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	auto resultSampler = vkCreateSampler(theirVulkanFramework.GetDevice(), &samplerInfo, nullptr, &mySampler);
	assert(!resultSampler && "failed creating sampler");

	// SAMPLER DESCRIPTOR
	//LAYOUT
	VkDescriptorSetLayoutBinding sampBinding;
	sampBinding.descriptorCount = 1;
	sampBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	sampBinding.binding = 0;
	sampBinding.pImmutableSamplers = nullptr;
	sampBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutCreateInfo sampLayoutInfo;
	sampLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	sampLayoutInfo.pNext = nullptr;
	sampLayoutInfo.flags = NULL;

	sampLayoutInfo.bindingCount = 1;
	sampLayoutInfo.pBindings = &sampBinding;

	auto resultSampLayout = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &sampLayoutInfo, nullptr, &mySamplerSetLayout);
	assert(!resultSampLayout && "failed creating image array descriptor set LAYOUT");

	// SET
	VkDescriptorSetAllocateInfo sampAllocInfo;
	sampAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	sampAllocInfo.pNext = nullptr;

	sampAllocInfo.descriptorSetCount = 1;
	sampAllocInfo.pSetLayouts = &mySamplerSetLayout;
	sampAllocInfo.descriptorPool = myDescriptorPool;

	auto resultSampAlloc = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &sampAllocInfo, &mySamplerSet);
	assert(!resultSampAlloc && "failed creating image array descriptor set");

	// WRITE SAMPLERS
	VkDescriptorImageInfo imageInfo{};
	imageInfo.sampler = mySampler;
	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstSet = mySamplerSet;
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.pImageInfo = &imageInfo;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	// DO DEFAULT TEXTURES

	RawDDS rawAlbedo = ReadDDS("Engine Assets/def_albedo.dds");

	auto [resultAlbedo, albedoView] = theirImageAllocator.RequestImage2D(rawAlbedo.images[0][0].data(),
																		  rawAlbedo.images[0][0].size(),
																		  rawAlbedo.width,
																		  rawAlbedo.height,
																		  1 + std::log2(std::max(rawAlbedo.width, rawAlbedo.height)),
																		  myOwners.data(),
																		  myOwners.size());

	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = albedoView;

		std::vector<VkDescriptorImageInfo> defAlbedoInfos;
		defAlbedoInfos.resize(MaxNumImages2D, imageInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstSet = myImageArraysSet;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorCount = MaxNumImages2D;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = defAlbedoInfos.data();
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}

	auto [resultStorage, storageView] = theirImageAllocator.RequestImage2D(nullptr,
																			0,
																			16,
																			16,
																			1,
																			myOwners.data(),
																			myOwners.size(),
																			VK_FORMAT_R8_UNORM,
																			VK_IMAGE_LAYOUT_GENERAL,
																			VK_IMAGE_USAGE_STORAGE_BIT
	);
	for (uint32_t i = 0; i < MaxNumStorageImages; ++i)
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = storageView;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstSet = myImageArraysSet;
		write.dstBinding = 1;
		write.dstArrayElement = i;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		write.pImageInfo = &imageInfo;
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}
	int val = 0;

	{
		RawDDS rawCube = ReadDDS("Engine Assets/def_cube.dds");

		const char* data[6]
		{
			rawCube.images[0][0].data(),
			rawCube.images[1][0].data(),
			rawCube.images[2][0].data(),
			rawCube.images[3][0].data(),
			rawCube.images[4][0].data(),
			rawCube.images[5][0].data(),
		};
		size_t bytes[6]
		{
			rawCube.images[0][0].size(),
			rawCube.images[1][0].size(),
			rawCube.images[2][0].size(),
			rawCube.images[3][0].size(),
			rawCube.images[4][0].size(),
			rawCube.images[5][0].size(),
		};
		auto [resultCube, cubeView] = theirImageAllocator.RequestImageCube(data,
																			bytes,
																			rawCube.width,
																			rawCube.height,
																			1 + std::log2(std::max(rawCube.width, rawCube.height)),
																			myOwners.data(),
																			myOwners.size());

		{
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = cubeView;

			std::vector<VkDescriptorImageInfo> cubeDescInfos;
			cubeDescInfos.resize(MaxNumImagesCube, imageInfo);

			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;

			write.dstSet = myImageArraysSet;
			write.dstBinding = 2;
			write.dstArrayElement = 0;
			write.descriptorCount = MaxNumImagesCube;
			write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			write.pImageInfo = cubeDescInfos.data();
			write.pBufferInfo = nullptr;
			write.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
		}
	}
	AddImage2D("brdfFilament.dds");
}

ImageHandler::~ImageHandler()
{
	vkDestroySampler(theirVulkanFramework.GetDevice(), mySampler, nullptr);
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), myImageArraysSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), mySamplerSetLayout, nullptr);
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

ImageID
ImageHandler::AddImage2D(const char* path)
{
	ImageID imgID = myImageIDKeeper.FetchFreeID();
	if (BAD_ID(imgID))
	{
		LOG("no more free image slots");
		return ImageID(INVALID_ID);
	}

	auto rawDDS = ReadDDS(path);

	VkResult result{};
	{
		std::tie(result, myImages2D[int(imgID)]) = theirImageAllocator.RequestImage2D(rawDDS.imagesInline.data(),
																						   rawDDS.images[0][0].size(),
																						   rawDDS.width,
																						   rawDDS.height,
																						   NUM_MIPS(std::max(rawDDS.width, rawDDS.height)),
																						   myOwners.data(),
																						   myOwners.size());
	}

	if (result)
	{
		LOG("failed loading image 2D, error code :", result);
		return ImageID(INVALID_ID);
	}

	myImages2DDims[uint32_t(imgID)] = {rawDDS.width, rawDDS.height};

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = myImages2D[int(imgID)];

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstSet = myImageArraysSet;
	write.dstBinding = 0;
	write.dstArrayElement = uint32_t(imgID);
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &imageInfo;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return imgID;
}

ImageID
ImageHandler::AddImage2D(
	std::vector<char>&& pixelData,
	VkFormat			format,
	uint32_t			numPixelVals)
{
	ImageID imgID = myImageIDKeeper.FetchFreeID();
	if (BAD_ID(imgID))
	{
		LOG("no more free image slots");
		return ImageID(INVALID_ID);
	}

	uint32_t dim = sqrtf(pixelData.size() / numPixelVals);
	VkResult result{};
	{
		std::tie(result, myImages2D[int(imgID)]) = theirImageAllocator.RequestImage2D(pixelData.data(),
																					  pixelData.size(),
																					  dim,
																					  dim,
																					  NUM_MIPS(dim),
																					  myOwners.data(),
																					  myOwners.size(),
																					  format);
	}

	if (result)
	{
		LOG("failed loading image 2D, error code :", result);
		return ImageID(INVALID_ID);
	}

	myImages2DDims[uint32_t(imgID)] = {dim, dim};

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = myImages2D[int(imgID)];

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.dstSet = myImageArraysSet;
	write.dstBinding = 0;
	write.dstArrayElement = uint32_t(imgID);
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return imgID;
}

ImageID
ImageHandler::AddImage2DArray(
	std::vector<char>&& pixelData, 
	Vec2f imageDim, 
	VkFormat format, 
	uint32_t numPixelVals)
{
	ImageID id = myImageIDKeeper.FetchFreeID();
	if (BAD_ID(id))
	{
		return id;
	}

	
	VkResult result;
	VkImageView fontView{};
	uint32_t numLayers = pixelData.size() / (imageDim.x * imageDim.y * numPixelVals);
	std::tie(result, fontView) = theirImageAllocator.RequestImageArray(pixelData,
																	   numLayers,
																	   imageDim.x,
																	   imageDim.y,
																	   NUM_MIPS(std::max(imageDim.x,imageDim.y)),
																	   myOwners.data(),
																	   myOwners.size(),
																	   format);


	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo.imageView = fontView;
	imgInfo.sampler = mySampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.dstSet = myImageArraysSet;
	write.dstBinding = 0;
	write.dstArrayElement = uint32_t(id);
	write.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	return id;
}

CubeID
ImageHandler::AddImageCube(const char* path)
{
	CubeID cubeID = myCubeIDKeeper.FetchFreeID();
	if (BAD_ID(cubeID))
	{
		LOG("no more free cube slots");
		return CubeID(INVALID_ID);
	}

	auto rawDDS = ReadDDS(path);

	VkResult result{};

	{
		const char* data[6]
		{
			rawDDS.images[0][0].data(),
			rawDDS.images[1][0].data(),
			rawDDS.images[2][0].data(),
			rawDDS.images[3][0].data(),
			rawDDS.images[4][0].data(),
			rawDDS.images[5][0].data(),
		};
		size_t bytes[6]
		{
			rawDDS.images[0][0].size(),
			rawDDS.images[1][0].size(),
			rawDDS.images[2][0].size(),
			rawDDS.images[3][0].size(),
			rawDDS.images[4][0].size(),
			rawDDS.images[5][0].size(),
		};
		std::tie(result, myImagesCube[int(cubeID)]) = theirImageAllocator.RequestImageCube(data,
																								bytes,
																								rawDDS.width,
																								rawDDS.height,
																								1 + std::log2(std::max(rawDDS.width, rawDDS.height)),
																								myOwners.data(),
																								myOwners.size());
	}

	if (result)
	{
		LOG("failed loading image cube, error code :", result);
		return CubeID(INVALID_ID);
	}

	myImagesCubeDims[uint32_t(cubeID)] = rawDDS.width;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = myImagesCube[int(cubeID)];

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstSet = myImageArraysSet;
	write.dstBinding = 2;
	write.dstArrayElement = uint32_t(cubeID);
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &imageInfo;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return cubeID;
}

VkResult
ImageHandler::AddStorageImage(
	uint32_t	index,
	VkFormat	format,
	uint32_t	width,
	uint32_t	height)
{
	auto [result, view] = theirImageAllocator.RequestImage2D(nullptr,
															  0,
															  width,
															  height,
															  1,
															  myOwners.data(),
															  myOwners.size(),
															  format,
															  VK_IMAGE_LAYOUT_GENERAL,
															  VK_IMAGE_USAGE_STORAGE_BIT);

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = view;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;

	write.dstSet = myImageArraysSet;
	write.dstBinding = 1;
	write.dstArrayElement = index;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imageInfo;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return result;
}

VkDescriptorSetLayout
ImageHandler::GetImageArraySetLayout() const
{
	return myImageArraysSetLayout;
}

VkDescriptorSet
ImageHandler::GetImageArraySet() const
{
	return myImageArraysSet;
}

VkDescriptorSetLayout
ImageHandler::GetSamplerSetLayout() const
{
	return mySamplerSetLayout;
}

VkDescriptorSet
ImageHandler::GetSamplerSet() const
{
	return mySamplerSet;
}

void
ImageHandler::BindSamplers(
	VkCommandBuffer		commandBuffer,
	VkPipelineLayout	pipelineLayout,
	uint32_t			setIndex,
	VkPipelineBindPoint bindPoint)
{
	vkCmdBindDescriptorSets(commandBuffer,
							bindPoint,
							pipelineLayout,
							setIndex,
							1,
							&mySamplerSet,
							0,
							nullptr);
}

void
ImageHandler::BindImages(
	VkCommandBuffer		commandBuffer,
	VkPipelineLayout	pipelineLayout,
	uint32_t			setIndex,
	VkPipelineBindPoint bindPoint)
{
	vkCmdBindDescriptorSets(commandBuffer,
							bindPoint,
							pipelineLayout,
							setIndex,
							1,
							&myImageArraysSet,
							0,
							nullptr);
}

VkImageView
ImageHandler::operator[](ImageID id)
{
	if (BAD_ID(id))
	{
		return nullptr;
	}
	return myImages2D[int(id)];
}

VkImageView
ImageHandler::operator[](CubeID id)
{
	if (BAD_ID(id))
	{
		return nullptr;
	}
	return myImagesCube[int(id)];
}

Vec2f
ImageHandler::GetImage2DDimension(ImageID id)
{
	if (BAD_ID(id))
	{
		return {};
	}
	return myImages2DDims[uint32_t(id)];
}

float
ImageHandler::GetImageCubeDimension(CubeID id)
{
	if (BAD_ID(id))
	{
		return 0.f;
	}
	return myImagesCubeDims[uint32_t(id)];
}

ImageAllocator&
ImageHandler::GetImageAllocator() const
{
	return theirImageAllocator;
}
