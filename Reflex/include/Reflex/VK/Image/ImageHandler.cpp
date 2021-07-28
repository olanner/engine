#include "pch.h"
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
	std::vector<uint8_t> checkers(64*64);
	for (uint32_t y = 0; y < 64; ++y)
	{
		for (uint32_t x = 0; x < 64; ++x)
		{
			uint32_t ty = y / 8;
			uint32_t tx = x / 8;
			checkers[y * 64 + x] = (ty + tx) % 2 * 255;
		}
	}

	auto [resultAlbedo, albedoView] = theirImageAllocator.RequestImageArray(
		checkers,
		1,
		64,
		64,
		NUM_MIPS(64),
		myOwners.data(),
		myOwners.size(),
		VK_FORMAT_R8_UNORM);

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
		checkers.resize(checkers.size()*6);
		std::vector<uint8_t> checkersCube;
		for (int i = 0; i < 6; ++i)
		{
			checkersCube.insert(checkersCube.end(), checkers.begin(), checkers.end());
		}
		auto [resultCube, cubeView] = theirImageAllocator.RequestImageCube(
			checkersCube,
			checkersCube.size()/6,
			64,
			64,
			NUM_MIPS(64),
			myOwners.data(),
			myOwners.size(),
			VK_FORMAT_R8_UNORM);

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

	myImageSwizzleToFormat[neat::ImageSwizzle::RGBA] = VK_FORMAT_R8G8B8A8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::RGB] = VK_FORMAT_R8G8B8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::ABGR] = VK_FORMAT_A8B8G8R8_UNORM_PACK32;
	myImageSwizzleToFormat[neat::ImageSwizzle::BGRA] = VK_FORMAT_B8G8R8A8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::BGR] = VK_FORMAT_B8G8R8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::R] = VK_FORMAT_R8_UNORM;
	
	myImageSwizzleToFormat[neat::ImageSwizzle::Unknown] = VK_FORMAT_UNDEFINED;
	
	AddImage2D("brdfFilament.dds");
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
	auto img = neat::ReadImage(path);

	return AddImage2D(
		std::move(img.pixelData), 
		{img.width, img.height}, 
		myImageSwizzleToFormat[img.swizzle],
		img.bitDepth / 8);
}

ImageID
ImageHandler::AddImage2D(
	std::vector<uint8_t>&& pixelData,
	Vec2f				dimension,
	VkFormat			format,
	uint32_t			byteDepth)
{
	ImageID imgID = myImageIDKeeper.FetchFreeID();
	if (BAD_ID(imgID))
	{
		LOG("no more free image slots");
		return ImageID(INVALID_ID);
	}
	uint32_t layers = pixelData.size() / (dimension.x * dimension.y * byteDepth);
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
ImageID
ImageHandler::AddImage2DTiled(
	const char* path,
	uint32_t	rows,
	uint32_t	cols)
{
	const auto img = neat::ReadImage(path);
	
	std::vector<uint8_t> sortedData;
	sortedData.reserve(img.pixelData.size());

	const uint32_t numTiles = rows * cols;
	const uint32_t tileWidth = img.width / cols;
	const uint32_t tileHeight = img.height / rows;
	const uint32_t bytesPerTile = img.pixelData.size() / numTiles;

	for (uint32_t tile = 0; tile < numTiles; ++tile)
	{
		uint32_t x = tile % cols * (tileWidth * 4);
		uint32_t y = tile / rows * tileHeight;
		for (uint32_t byte = 0; byte < bytesPerTile; byte++)
		{
			uint32_t ix = x + byte % (tileWidth * 4);
			uint32_t iy = y + byte / (tileWidth * 4);
			uint32_t index = iy * (img.width * 4) + ix;

			sortedData.emplace_back(img.pixelData[index]);

		}
	}
	
	return AddImage2D(std::move(sortedData), {tileWidth, tileHeight}, myImageSwizzleToFormat[img.swizzle]);
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

	auto img = neat::ReadImage(path);
	if (img.layers != 6)
	{
		LOG(path, "is not a cube map image");
		return CubeID(INVALID_ID);
	}

	VkResult result{};
	{
		std::tie(result, myImagesCube[int(cubeID)]) = 
			theirImageAllocator.RequestImageCube(
			img.pixelData,
			img.pixelData.size() / 6,
			img.width,
			img.height,
			1 + std::log2(std::max(img.width, img.height)),
			myOwners.data(),
			myOwners.size());
	}

	if (result)
	{
		LOG("failed loading image cube, error code :", result);
		return CubeID(INVALID_ID);
	}

	myImagesCubeDims[uint32_t(cubeID)] = img.width;

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
