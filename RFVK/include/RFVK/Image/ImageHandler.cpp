#include "pch.h"
#include "ImageHandler.h"

#include "RFVK/VulkanFramework.h"
#include "RFVK/Memory/ImageAllocator.h"
#define STB_IMAGE_IMPLEMENTATION
#include "RFVK/Misc/stb/stb_image.h"
#include <RFVK/Debug/DebugUtils.h>

ImageHandler::ImageHandler(
	VulkanFramework&	vulkanFramework,
	ImageAllocator&		imageAllocator,
	QueueFamilyIndices		familyIndices)
	: HandlerBase(vulkanFramework)
	, theirImageAllocator(imageAllocator)
	, myImageIDKeeper(MaxNumImages)
	, myCubeIDKeeper(MaxNumImagesCube)
{
	myOwners = {
		familyIndices[QUEUE_FAMILY_TRANSFER],
		familyIndices[QUEUE_FAMILY_GRAPHICS],
		familyIndices[QUEUE_FAMILY_COMPUTE],
	};

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

	std::array bindings
	{
		images2DBinding,
		cubeMapArrBinding,
		imagesStorageBinding,
	};

	imgArrLayoutInfo.bindingCount = bindings.size();
	imgArrLayoutInfo.pBindings = bindings.data();

	auto resultLayout = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &imgArrLayoutInfo, nullptr, &myImageSetLayout);
	assert(!resultLayout && "failed creating image array descriptor set LAYOUT");

	// POOL
	VkDescriptorPoolSize sampledImageDescriptorSize;
	sampledImageDescriptorSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	sampledImageDescriptorSize.descriptorCount = (MaxNumImages + MaxNumImagesCube) * NumSwapchainImages * 2;
	VkDescriptorPoolSize storageImageDescriptorSize;
	storageImageDescriptorSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	storageImageDescriptorSize.descriptorCount = MaxNumStorageImages * NumSwapchainImages * 2;
	VkDescriptorPoolSize samplerDescriptorsSize;
	samplerDescriptorsSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerDescriptorsSize.descriptorCount = MaxNumSamplers;

	std::array poolSizes
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
	poolInfo.maxSets = NumSwapchainImages * bindings.size() * 2; // times 2 just in case

	auto resultPool = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!resultPool && "failed creating pool");

	// SET
	VkDescriptorSetAllocateInfo imgArrAllocInfo;
	imgArrAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	imgArrAllocInfo.pNext = nullptr;

	imgArrAllocInfo.descriptorSetCount = 1;
	imgArrAllocInfo.pSetLayouts = &myImageSetLayout;
	imgArrAllocInfo.descriptorPool = myDescriptorPool;

	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		auto resultAlloc = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &imgArrAllocInfo, &myImageSets[swapchainIndex]);
		assert(!resultAlloc && "failed creating image array descriptor set");
	}

	// SAMPLERS
	CreateSampler(VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 16);
	CreateSampler(VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0);

	// SAMPLER DESCRIPTOR
	//LAYOUT
	VkDescriptorSetLayoutBinding sampBinding;
	sampBinding.descriptorCount = mySamplers.size();
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
	{
		std::vector<VkDescriptorImageInfo> samplerDescInfos;
		for (auto&& sampler : mySamplers)
		{
			VkDescriptorImageInfo info = {};
			info.sampler = sampler;
			samplerDescInfos.emplace_back(info);
		}

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstSet = mySamplerSet;
		write.dstBinding = SamplerSetSamplersBinding;
		write.dstArrayElement = 0;
		write.descriptorCount = samplerDescInfos.size();
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		write.pImageInfo = samplerDescInfos.data();
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}

	// DO DEFAULT TEXTURES
	auto allocSubID = theirImageAllocator.Start();

	std::vector<Vec4uc> checkersPix(64*64);
	for (uint32_t y = 0; y < 64; ++y)
	{
		for (uint32_t x = 0; x < 64; ++x)
		{
			uint32_t ty = y / 8;
			uint32_t tx = x / 8;
			unsigned char value = (ty + tx) % 2 * 255;
			checkersPix[y * 64 + x] = Vec4uc(
				value + (tx+ty)/16.f * 255.f * !value,
				value + (tx+ty)/16.f * 255.f * !value,
				value + (tx+ty)/16.f * 255.f * !value,
				255);
		}
	}
	std::vector<uint8_t> checkers(checkersPix.size() * 4);
	memcpy_s(checkers.data(), checkers.size(), checkersPix.data(), checkersPix.size() * 4);

	{
		ImageRequestInfo requestInfo;
		requestInfo.width = 64;
		requestInfo.height = 64;
		requestInfo.mips = NUM_MIPS(64);
		requestInfo.owners = myOwners;
		auto [resultAlbedo, albedoView] = theirImageAllocator.RequestImageArray(
			allocSubID,
			checkers,
			1,
			requestInfo);
		
		myDefaultImage2DInfo = {};
		myDefaultImage2DInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		myDefaultImage2DInfo.imageView = albedoView;

		std::vector<VkDescriptorImageInfo> defAlbedoInfos;
		defAlbedoInfos.resize(MaxNumImages, myDefaultImage2DInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;
		
		write.dstBinding = ImageSetImages2DBinding;
		write.dstArrayElement = 0;
		write.descriptorCount = MaxNumImages;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = defAlbedoInfos.data();
		write.pBufferInfo = nullptr;
		write.pTexelBufferView = nullptr;

		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			write.dstSet = myImageSets[swapchainIndex];
			vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
		}

		myDefaultImage2DWrite = write;
		myDefaultImage2DWrite.pImageInfo = &myDefaultImage2DInfo;
		myDefaultImage2DWrite.descriptorCount = 1;
		myDefaultImage2DWrite.dstSet = nullptr;
	}
	{
		ImageRequestInfo requestInfo;
		requestInfo.width = 4;
		requestInfo.height = 4;
		requestInfo.mips = 1;
		requestInfo.owners = myOwners;
		requestInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
		requestInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;

		auto [resultStorage, storageView] = theirImageAllocator.RequestImage2D(
			allocSubID,
			nullptr,
			16,
			requestInfo);

		for (uint32_t i = 0; i < MaxNumStorageImages; ++i)
		{
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.imageView = storageView;

			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;

			write.dstBinding = ImageSetImagesStorageBinding;
			write.dstArrayElement = i;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write.pImageInfo = &imageInfo;
			write.pBufferInfo = nullptr;
			write.pTexelBufferView = nullptr;

			for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
			{
				write.dstSet = myImageSets[swapchainIndex];
				vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
			}
		}
	}

	{
		checkers.resize(checkers.size()*6);
		std::vector<uint8_t> checkersCube;
		for (int i = 0; i < 6; ++i)
		{
			checkersCube.insert(checkersCube.end(), checkers.begin(), checkers.end());
		}
		ImageRequestInfo requestInfo;
		requestInfo.width = 64;
		requestInfo.height = 64;
		requestInfo.mips = NUM_MIPS(64);
		requestInfo.owners = myOwners;
		auto [resultCube, cubeView] = theirImageAllocator.RequestImageCube(
			allocSubID,
			checkersCube,
			checkersCube.size() / 6,
			requestInfo
		);
		ImageCube cube;
		cube.info.imageView = cubeView;
		cube.info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		cube.view = cubeView;
		cube.dim = 64;
		myImagesCube.fill(cube);
		
		{
			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = cubeView;

			std::vector<VkDescriptorImageInfo> cubeDescInfos;
			cubeDescInfos.resize(MaxNumImagesCube, imageInfo);

			VkWriteDescriptorSet write{};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;

			write.dstBinding = ImageSetImagesCubeBinding;
			write.dstArrayElement = 0;
			write.descriptorCount = MaxNumImagesCube;
			write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			write.pImageInfo = cubeDescInfos.data();
			write.pBufferInfo = nullptr;
			write.pTexelBufferView = nullptr;

			for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
			{
				write.dstSet = myImageSets[swapchainIndex];
				vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
			}
		}
	}

	myImageSwizzleToFormat[neat::ImageSwizzle::RGBA] = VK_FORMAT_R8G8B8A8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::RGB] = VK_FORMAT_R8G8B8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::ABGR] = VK_FORMAT_A8B8G8R8_UNORM_PACK32;
	myImageSwizzleToFormat[neat::ImageSwizzle::BGRA] = VK_FORMAT_B8G8R8A8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::BGR] = VK_FORMAT_B8G8R8_UNORM;
	myImageSwizzleToFormat[neat::ImageSwizzle::R] = VK_FORMAT_R8_UNORM;
	
	myImageSwizzleToFormat[neat::ImageSwizzle::Unknown] = VK_FORMAT_UNDEFINED;
	
	LoadImage2D(AddImage2D(), allocSubID, "brdfFilament.tga");
	theirImageAllocator.Queue(std::move(allocSubID));
}

ImageHandler::~ImageHandler()
{
	for (auto& sampler : mySamplers)
	{
		vkDestroySampler(theirVulkanFramework.GetDevice(), sampler, nullptr);
	}
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), myImageSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(theirVulkanFramework.GetDevice(), mySamplerSetLayout, nullptr);
	vkDestroyDescriptorPool(theirVulkanFramework.GetDevice(), myDescriptorPool, nullptr);
}

ImageID
ImageHandler::AddImage2D()
{
	ImageID imageID = myImageIDKeeper.FetchFreeID();
	if (BAD_ID(imageID))
	{
		LOG("no more free image slots");
		return ImageID(INVALID_ID);
	}
	return imageID;
}

void
ImageHandler::RemoveImage2D(
	ImageID imageID)
{
	UnloadImage2D(imageID);
	myImageIDKeeper.ReturnID(imageID);
}

void
ImageHandler::UnloadImage2D(
	ImageID imageID)
{
	if (BAD_ID(imageID))
	{
		LOG("tried to unload invalid image id");
		return;
	}
	if (myImages2D[int(imageID)].view == nullptr)
	{
		LOG("failed to unload image with id: ", int(imageID), ", already unloaded");
		return;
	}
	const std::shared_ptr<std::counting_semaphore<NumSwapchainImages>> doneSignal = std::make_shared<std::counting_semaphore<NumSwapchainImages>>(0);
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		auto write = myDefaultImage2DWrite;
		write.dstSet = myImageSets[swapchainIndex];
		write.dstArrayElement = int(imageID);
		QueueDescriptorUpdate(
			swapchainIndex,
			nullptr,
			doneSignal,
			write);
	}
	theirImageAllocator.QueueDestroy(std::move(myImages2D[int(imageID)].view), doneSignal);
	myImages2D[int(imageID)] = {};
}

void
ImageHandler::LoadImage2D(
	ImageID					imageID,
	AllocationSubmissionID	allocSubID,
	const std::string&		path)
{
	auto [pixels, x, y, channels] = ReadImage(path);
	
	return LoadImage2D(
		imageID,
		allocSubID,
		std::move(pixels), 
		{x,y}, 
		VK_FORMAT_R8G8B8A8_UNORM,
		4);
}

void
ImageHandler::LoadImage2D(
	ImageID					imageID,
	AllocationSubmissionID	allocSubID,
	std::vector<uint8_t>&&	pixelData,
	Vec2f					dimension,
	VkFormat				format,
	uint32_t				byteDepth)
{
	if (BAD_ID(imageID))
	{
		return;
	}
	myImages2D[uint32_t(imageID)] = {};
	
	uint32_t layers = pixelData.size() / (dimension.x * dimension.y * byteDepth);
	VkResult result{};
	{
		ImageRequestInfo requestInfo;
		requestInfo.width = dimension.x;
		requestInfo.height = dimension.y;
		requestInfo.mips = NUM_MIPS(std::max(dimension.x, dimension.y));
		requestInfo.owners = myOwners;
		requestInfo.format = format;
		auto& imageView = myImages2D[uint32_t(imageID)].view;
		std::tie(result, imageView) = theirImageAllocator.RequestImageArray(
			allocSubID,
			pixelData,
			layers,
			requestInfo);
		DebugSetObjectName(
			std::string("imgHndlr imgID ").append(std::to_string(int(imageID))).c_str(),
			theirImageAllocator.GetImage(imageView), 
			VK_OBJECT_TYPE_IMAGE_VIEW, 
			theirVulkanFramework.GetDevice());
	}

	if (result)
	{
		LOG("failed loading image 2D, error code: ", result);
	}

	myImages2D[uint32_t(imageID)].dim = dimension;
	auto [sw, sh] = theirVulkanFramework.GetTargetResolution();
	myImages2D[uint32_t(imageID)].scale = {dimension.x / sw, dimension.y / sh};
	myImages2D[uint32_t(imageID)].scale.x *= sw / sh;
	myImages2D[uint32_t(imageID)].layers = layers;
	
	myImages2D[uint32_t(imageID)].info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	myImages2D[uint32_t(imageID)].info.imageView = myImages2D[uint32_t(imageID)].view;

	auto& allocSub = theirImageAllocator.GetAllocationSubmission(allocSubID);
	auto executedEvent = allocSub.GetExecutedEvent();
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding = ImageSetImages2DBinding;
		write.dstArrayElement = uint32_t(imageID);
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = &myImages2D[uint32_t(imageID)].info;
		write.dstSet = myImageSets[swapchainIndex];
		QueueDescriptorUpdate(
			swapchainIndex, 
			executedEvent,
			nullptr,
			write);
		//myQueuedDescriptorWrites[swapchainIndex].push({ executedEvent, write });
	}
}
void
ImageHandler::LoadImage2DTiled(
	ImageID					imageID,
	AllocationSubmissionID	allocSubID,
	const std::string&		path,
	uint32_t				rows,
	uint32_t				cols)
{
	auto [pixels, width, height, channels] = ReadImage(path);

	if (pixels.empty())
	{
		LOG("image file,", path, "was empty");
		return;
	}
	
	std::vector<uint8_t> sortedData;
	sortedData.reserve(pixels.size());

	const uint32_t numTiles = rows * cols;
	const uint32_t tileWidth = width / cols;
	const uint32_t tileHeight = height / rows;
	const uint32_t bytesPerTile = pixels.size() / numTiles;

	for (uint32_t tile = 0; tile < numTiles; ++tile)
	{
		uint32_t x = tile % cols * (tileWidth * 4);
		uint32_t y = tile / rows * tileHeight;
		for (uint32_t byte = 0; byte < bytesPerTile; byte++)
		{
			uint32_t ix = x + byte % (tileWidth * 4);
			uint32_t iy = y + byte / (tileWidth * 4);
			uint32_t index = iy * (width * 4) + ix;

			sortedData.emplace_back(pixels[index]);

		}
	}
	
	return LoadImage2D(
		imageID, 
		allocSubID, 
		std::move(sortedData), 
		{tileWidth, tileHeight}, 
VK_FORMAT_R8G8B8A8_UNORM);
}

CubeID ImageHandler::AddImageCube()
{
	CubeID cubeID = myCubeIDKeeper.FetchFreeID();
	if (BAD_ID(cubeID))
	{
		LOG("no more free cube slots");
		return CubeID(INVALID_ID);
	}
    return cubeID;
}

shared_semaphore<NumSwapchainImages>
ImageHandler::LoadImageCube(
	CubeID					cubeID,
	AllocationSubmissionID	allocSubID,
	const std::string&		path)
{
	auto& imageCube = myImagesCube[int(cubeID)];

	auto img = neat::ReadImage(path.c_str());
	if (img.layers != 6)
	{
		LOG(path, "is not a cube map image");
		return nullptr;
	}
	if (int(img.error))
	{
		LOG("failed loading image, ", path);
		return nullptr;
	}

	VkResult result{};
	{
		ImageRequestInfo requestInfo;
		requestInfo.width = img.width;
		requestInfo.height = img.height;
		requestInfo.mips = NUM_MIPS(std::max(img.width, img.height));
		requestInfo.owners = myOwners;
		std::tie(result, imageCube.view) = 
			theirImageAllocator.RequestImageCube(
			allocSubID,
			img.pixelData,
			img.pixelData.size() / 6,
			requestInfo);
	}

	if (result)
	{
		LOG("failed loading image cube, error code :", result);
		return nullptr;
	}

	imageCube.dim = img.width;

	imageCube.info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageCube.info.imageView = imageCube.view;

	auto& allocSub = theirImageAllocator.GetAllocationSubmission(allocSubID);
	auto executedEvent = allocSub.GetExecutedEvent();
	auto doneSignal = std::make_shared<shared_semaphore<NumSwapchainImages>::element_type>(0);
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstBinding = ImageSetImagesCubeBinding;
		write.dstArrayElement = uint32_t(cubeID);
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = &imageCube.info;
		write.dstSet = myImageSets[swapchainIndex];
		QueueDescriptorUpdate(
			swapchainIndex, 
			executedEvent,
			doneSignal,
			write);
		//myQueuedDescriptorWrites[swapchainIndex].push({executedEvent, write});
	}

	return doneSignal;
}

VkResult
ImageHandler::LoadStorageImage(
	uint32_t	index,
	VkFormat	format,
	uint32_t	width,
	uint32_t	height)
{
	AllocationSubmissionID allocSubID = theirImageAllocator.Start();

	ImageRequestInfo requestInfo;
	requestInfo.width = width;
	requestInfo.height = height;
	requestInfo.format = format;
	requestInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
	requestInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT;
	requestInfo.owners = myOwners;
	auto [result, view] = theirImageAllocator.RequestImage2D(
		allocSubID,
		nullptr,
		0,
		requestInfo);

	theirImageAllocator.Queue(std::move(allocSubID));

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.imageView = view;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstBinding = ImageSetImagesStorageBinding;
	write.dstArrayElement = index;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.pImageInfo = &imageInfo;

	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		write.dstSet = myImageSets[swapchainIndex];
		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}

	return result;
}

VkDescriptorSetLayout
ImageHandler::GetImageSetLayout() const
{
	return myImageSetLayout;
}

VkDescriptorSetLayout
ImageHandler::GetSamplerSetLayout() const
{
	return mySamplerSetLayout;
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
	int					swapchainIndex,
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
		&myImageSets[swapchainIndex],
		0,
		nullptr);
}

Image
ImageHandler::operator[](ImageID id)
{
	if (BAD_ID(id))
	{
		return {};
	}
	return myImages2D[uint32_t(id)];
}

ImageCube
ImageHandler::operator[](CubeID id)
{
	if (BAD_ID(id))
	{
		return {};
	}
	return myImagesCube[uint32_t(id)];
}

std::tuple<std::vector<uint8_t>, int, int, int>
ImageHandler::ReadImage(
	const std::string& path)
{
	int x, y, channels;
	auto img = stbi_load(path.c_str(), &x, &y, &channels, 4);
	if (!img)
	{
		LOG("failed loading image, \"", path, "\"");
		return {};
	}
	std::vector<uint8_t> pixels;
	pixels.insert(pixels.end(), img, img + x * y * 4);
	stbi_image_free(img);
	return {pixels, x, y, channels};
}

void 
ImageHandler::CreateSampler(
				VkFilter				filter, 
				VkSamplerAddressMode	samplerMode,
				float					anisotropy)
{
	assert(mySamplers.emplace_back(nullptr) && "max samplers reached");

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.pNext = nullptr;
	samplerInfo.flags = NULL;

	samplerInfo.magFilter = filter;
	samplerInfo.minFilter = filter;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = samplerMode;
	samplerInfo.addressModeV = samplerMode;
	samplerInfo.addressModeW = samplerMode;
	samplerInfo.mipLodBias = 0;
	samplerInfo.anisotropyEnable = !!anisotropy;
	samplerInfo.maxAnisotropy = anisotropy;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.minLod = 0.f;
	samplerInfo.maxLod = FLT_MAX;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	auto resultSampler = vkCreateSampler(theirVulkanFramework.GetDevice(), &samplerInfo, nullptr, &mySamplers.back());
	assert(!resultSampler && "failed creating sampler");
}

ImageAllocator&
ImageHandler::GetImageAllocator() const
{
	return theirImageAllocator;
}
