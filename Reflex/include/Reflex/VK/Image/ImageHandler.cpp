#include "pch.h"
#include "neat/Image/DDSReader.h"
#include "ImageHandler.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/ImageAllocator.h"

ImageHandler::ImageHandler(
	VulkanFramework& vulkanFramework,
	ImageAllocator& imageAllocator,
	QueueFamilyIndex* firstOwner,
	uint32_t			numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirImageAllocator(imageAllocator)
	, myDescriptorPool()
	, myImageSet()
	, myImageSetLayout()
	, mySampler()
	, mySamplerSet()
	, mySamplerSetLayout()
	, myImages2D{}
	, myImageIDKeeper(MaxNumImages)
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
	sampledImageDescriptorSize.descriptorCount = MaxNumImages + MaxNumImagesCube;
	VkDescriptorPoolSize storageImageDescriptorSize;
	storageImageDescriptorSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageImageDescriptorSize.descriptorCount = MaxNumStorageImages;
	VkDescriptorPoolSize samplerDescriptorsSize;
	samplerDescriptorsSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerDescriptorsSize.descriptorCount = MaxNumSamplers;
	VkDescriptorPoolSize image2DArrayDescriptorSize;

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
	VkDescriptorSetLayoutBinding images2DBinding;
	images2DBinding.descriptorCount = MaxNumImages;
	images2DBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	images2DBinding.binding = ImageSetImages2DBinding;
	images2DBinding.pImmutableSamplers = nullptr;
	images2DBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutBinding cubeMapArrBinding;
	cubeMapArrBinding.descriptorCount = MaxNumImagesCube;
	cubeMapArrBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	cubeMapArrBinding.binding = ImageSetImagesCubeBinding;
	cubeMapArrBinding.pImmutableSamplers = nullptr;
	cubeMapArrBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutBinding imagesStorageBinding;
	imagesStorageBinding.descriptorCount = MaxNumStorageImages;
	imagesStorageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	imagesStorageBinding.binding = ImageSetImagesStorageBinding;
	imagesStorageBinding.pImmutableSamplers = nullptr;
	imagesStorageBinding.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutCreateInfo imgArrLayoutInfo;
	imgArrLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	imgArrLayoutInfo.pNext = nullptr;
	imgArrLayoutInfo.flags = NULL;

	VkDescriptorSetLayoutBinding bindings[]
	{
		images2DBinding,
		cubeMapArrBinding,
		imagesStorageBinding,
	};

	imgArrLayoutInfo.bindingCount = ARRAYSIZE(bindings);
	imgArrLayoutInfo.pBindings = bindings;

	auto resultLayout = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &imgArrLayoutInfo, nullptr, &myImageSetLayout);
	assert(!resultLayout && "failed creating image array descriptor set LAYOUT");

	// SET
	VkDescriptorSetAllocateInfo imgArrAllocInfo;
	imgArrAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	imgArrAllocInfo.pNext = nullptr;

	imgArrAllocInfo.descriptorSetCount = 1;
	imgArrAllocInfo.pSetLayouts = &myImageSetLayout;
	imgArrAllocInfo.descriptorPool = myDescriptorPool;

	auto resultAlloc = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &imgArrAllocInfo, &myImageSet);
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
	samplerInfo.maxLod = FLT_MAX;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	auto resultSampler = vkCreateSampler(theirVulkanFramework.GetDevice(), &samplerInfo, nullptr, &mySampler);
	assert(!resultSampler && "failed creating sampler");

	// SAMPLER DESCRIPTOR
	//LAYOUT
	VkDescriptorSetLayoutBinding sampBinding;
	sampBinding.descriptorCount = 1;
	sampBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	sampBinding.binding = SamplerSetSamplersBinding;
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
	write.dstBinding = SamplerSetSamplersBinding;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	write.pImageInfo = &imageInfo;
	write.pBufferInfo = nullptr;
	write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	// DO DEFAULT TEXTURES

	RawDDS rawAlbedo = ReadDDS("Engine Assets/def_albedo.dds");

	auto [resultAlbedo, albedoView] = theirImageAllocator.RequestImageArray(rawAlbedo.images[0][0],
		1,
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
		defAlbedoInfos.resize(MaxNumImages, imageInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstSet = myImageSet;
		write.dstBinding = ImageSetImages2DBinding;
		write.dstArrayElement = 0;
		write.descriptorCount = MaxNumImages;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = defAlbedoInfos.data();
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}

	//auto imgArrData = rawAlbedo.images[0][0];
	//imgArrData.resize(imgArrData.size() * 2);
	//auto [resultDefImgArr, imgArrView] =
	//	theirImageAllocator.RequestImageArray(imgArrData,
	//		2,
	//		rawAlbedo.width,
	//		rawAlbedo.height,
	//		1 + std::log2(std::max(rawAlbedo.width, rawAlbedo.height)),
	//		myOwners.data(),
	//		myOwners.size());

	//{
	//	VkDescriptorImageInfo imgArrInfo{};
	//	imgArrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//	imgArrInfo.imageView = imgArrView;

	//	std::vector<VkDescriptorImageInfo> defImgArrInfos;
	//	defImgArrInfos.resize(MaxNumImages, imgArrInfo);

	//	VkWriteDescriptorSet write{};
	//	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	//	write.pNext = nullptr;

	//	write.dstSet = myImageSet;
	//	write.dstBinding = ImageSetImages2DArrayBinding;
	//	write.dstArrayElement = 0;
	//	write.descriptorCount = MaxNumImages2DArray;
	//	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	//	write.pImageInfo = defImgArrInfos.data();
	//	write.pBufferInfo = nullptr;
	//	write.pTexelBufferView = nullptr;

	//	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	//}

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

		write.dstSet = myImageSet;
		write.dstBinding = ImageSetImagesStorageBinding;
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
		
		auto [resultCube, cubeView] = theirImageAllocator.RequestImageCube(
			rawCube.imagesInline,
			rawCube.imagesInline.size()/6,
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

			write.dstSet = myImageSet;
			write.dstBinding = ImageSetImagesCubeBinding;
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
	AddImage2DTiled("Engine Assets/def_albedo.dds", 8, 8);
}

ImageHandler::~ImageHandler()
{
	vkDestroySampler(theirVulkanFramework.GetDevice(), mySampler, nullptr);
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), myImageSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), mySamplerSetLayout, nullptr);
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

ImageID
ImageHandler::AddImage2D(const char* path)
{
	auto rawDDS = ReadDDS(path);

	return AddImage2D(std::move(rawDDS.imagesInline), {rawDDS.width, rawDDS.height});
}

ImageID
ImageHandler::AddImage2D(
	std::vector<char>&& pixelData,
	Vec2f				dimension,
	VkFormat			format,
	uint32_t			numPixelVals)
{
	ImageID imgID = myImageIDKeeper.FetchFreeID();
	if (BAD_ID(imgID))
	{
		LOG("no more free image slots");
		return ImageID(INVALID_ID);
	}
	uint32_t layers = pixelData.size() / (dimension.x * dimension.y * numPixelVals);
	VkResult result{};
	{
		std::tie(result, myImages2D[int(imgID)]) = theirImageAllocator.RequestImageArray(
			pixelData,
					layers,
			dimension.x,
			dimension.y,
			NUM_MIPS(std::max(dimension.x, dimension.y)),
			myOwners.data(),
			myOwners.size(),
			format);
	}

	if (result)
	{
		LOG("failed loading image 2D, error code :", result);
		return ImageID(INVALID_ID);
	}

	myImages2DDims[uint32_t(imgID)] = dimension;

	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = myImages2D[int(imgID)];

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.dstSet = myImageSet;
	write.dstBinding = ImageSetImages2DBinding;
	write.dstArrayElement = uint32_t(imgID);
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);

	return imgID;
}

//ImageArrayID
//ImageHandler::AddImage2DArray(
//	std::vector<char>&& pixelData,
//	Vec2f imageDim,
//	VkFormat format,
//	uint32_t numPixelVals)
//{
//	ImageArrayID id = myImages2DArrayIDKeeper.FetchFreeID();
//	if (BAD_ID(id))
//	{
//		return id;
//	}
//
//
//	VkResult result;
//	VkImageView fontView{};
//	uint32_t numLayers = pixelData.size() / (imageDim.x * imageDim.y * numPixelVals);
//	std::tie(result, fontView) = theirImageAllocator.RequestImageArray(pixelData,
//		numLayers,
//		imageDim.x,
//		imageDim.y,
//		NUM_MIPS(std::max(imageDim.x, imageDim.y)),
//		myOwners.data(),
//		myOwners.size(),
//		format);
//
//
//	VkDescriptorImageInfo imgInfo{};
//	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//	imgInfo.imageView = fontView;
//
//	VkWriteDescriptorSet write{};
//	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//
//	write.descriptorCount = 1;
//	write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
//	write.dstSet = myImageSet;
//	write.dstBinding = ImageSetImages2DArrayBinding;
//	write.dstArrayElement = uint32_t(id);
//	write.pImageInfo = &imgInfo;
//
//	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
//	return id;
//}

ImageID
ImageHandler::AddImage2DTiled(
	const char* path,
	uint32_t	rows,
	uint32_t	cols)
{
	const auto rawDDS = ReadDDS(path);
	
	std::vector<char> sortedData;
	sortedData.reserve(rawDDS.imagesInline.size());

	const uint32_t numTiles = rows * cols;
	const uint32_t tileWidth = rawDDS.width / cols;
	const uint32_t tileHeight = rawDDS.height / rows;
	const uint32_t bytesPerTile = rawDDS.imagesInline.size() / numTiles;

	for (uint32_t tile = 0; tile < numTiles; ++tile)
	{
		uint32_t x = tile % cols * (tileWidth * 4);
		uint32_t y = tile / rows * tileHeight;
		for (uint32_t byte = 0; byte < bytesPerTile; byte++)
		{
			uint32_t ix = x + byte % (tileWidth * 4);
			uint32_t iy = y + byte / (tileWidth * 4);
			uint32_t index = iy * (rawDDS.width * 4) + ix;

			sortedData.emplace_back(rawDDS.imagesInline[index]);

		}
	}
	
	return AddImage2D(std::move(sortedData), {tileWidth, tileHeight});
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
	if (rawDDS.numLayers != 6)
	{
		LOG(path, "is not a cube map image");
		return CubeID(INVALID_ID);
	}

	VkResult result{};

	{
		/*const char* data[6]
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
		};*/
		std::tie(result, myImagesCube[int(cubeID)]) = 
			theirImageAllocator.RequestImageCube(
			rawDDS.imagesInline,
			rawDDS.imagesInline.size() / 6,
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

	write.dstSet = myImageSet;
	write.dstBinding = ImageSetImagesCubeBinding;
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

	write.dstSet = myImageSet;
	write.dstBinding = ImageSetImagesStorageBinding;
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
ImageHandler::GetImageSetLayout() const
{
	return myImageSetLayout;
}

VkDescriptorSet
ImageHandler::GetImageSet() const
{
	return myImageSet;
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
		&myImageSet,
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
