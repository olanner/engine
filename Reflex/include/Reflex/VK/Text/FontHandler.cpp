#include "pch.h"
#include "FontHandler.h"


#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/ImageAllocator.h"

#ifdef _DEBUG
#pragma comment(lib, "freetype-d.lib")
#else
#pragma comment(lib, "freetype.lib")
#endif

FT_Library FTLibrary;

FontHandler::FontHandler(
	VulkanFramework& vulkanFramework,
	ImageAllocator& imageAllocator,
	QueueFamilyIndex* firstOwner,
	uint32_t numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirImageAllocator(imageAllocator)
	, myFontIDKeeper(MaxNumFonts)
{
	myOwners.resize(numOwners);
	for (uint32_t ownerIndex = 0; ownerIndex < numOwners; ++ownerIndex)
	{
		myOwners[ownerIndex] = firstOwner[ownerIndex];
	}

	auto result = FT_Init_FreeType(&FTLibrary);
	assert(!result && "failed initializing free type");

	//	DESCRIPTOR SET POOL
	VkDescriptorPoolSize sampledFontsDescriptorSize;
	sampledFontsDescriptorSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampledFontsDescriptorSize.descriptorCount = MaxNumFonts;

	std::array<VkDescriptorPoolSize, 1> poolSizes
	{
		sampledFontsDescriptorSize
	};

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = NULL;

	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	result = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!result && "failed creating pool");

	//	IMAGE ARRAYS DESCRIPTOR LAYOUT
	VkDescriptorSetLayoutBinding sampledFonts;
	sampledFonts.descriptorCount = MaxNumFonts;
	sampledFonts.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampledFonts.binding = 0;
	sampledFonts.pImmutableSamplers = nullptr;
	sampledFonts.stageFlags =
		VK_SHADER_STAGE_FRAGMENT_BIT |
		VK_SHADER_STAGE_VERTEX_BIT |
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;

	VkDescriptorSetLayoutCreateInfo fontArraySetLayout;
	fontArraySetLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	fontArraySetLayout.pNext = nullptr;
	fontArraySetLayout.flags = NULL;

	VkDescriptorSetLayoutBinding bindings[]
	{
		sampledFonts,
	};

	fontArraySetLayout.bindingCount = ARRAYSIZE(bindings);
	fontArraySetLayout.pBindings = bindings;

	result = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &fontArraySetLayout, nullptr, &myFontArraySetLayout);
	assert(!result && "failed creating image array descriptor set LAYOUT");

	// FONT ARRAY SET
	VkDescriptorSetAllocateInfo fontArrayAllocInfo;
	fontArrayAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	fontArrayAllocInfo.pNext = nullptr;

	fontArrayAllocInfo.descriptorSetCount = 1;
	fontArrayAllocInfo.pSetLayouts = &myFontArraySetLayout;
	fontArrayAllocInfo.descriptorPool = myDescriptorPool;

	result = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &fontArrayAllocInfo, &myFontArraySet);
	assert(!result && "failed creating image array descriptor set");

	//	SAMPLER
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.anisotropyEnable = true;
	samplerInfo.maxLod = 16.0;
	samplerInfo.maxAnisotropy = 8;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	result = vkCreateSampler(theirVulkanFramework.GetDevice(), &samplerInfo, nullptr, &mySampler);
	assert(!result && "failed creating font sampler");

	//	DEFAULT FONT
	{
		VkImageView fontView = nullptr;
		std::tie(result, fontView) = theirImageAllocator.RequestImageArray({},
																		   LastFontGlyph - FirstFontGlyph + 1,
																		   32,
																		   32,
																		   1,
																		   myOwners.data(),
																		   myOwners.size(),
																		   VK_FORMAT_R8_UNORM);
		assert(!result && "failed creating default font");

		VkDescriptorImageInfo imgInfo{};
		imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imgInfo.imageView = fontView;
		imgInfo.sampler = mySampler;
		std::array<VkDescriptorImageInfo, MaxNumFonts> imgInfos;
		imgInfos.fill(imgInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

		write.descriptorCount = MaxNumFonts;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.dstSet = myFontArraySet;
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.pImageInfo = imgInfos.data();

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	}

}

FontHandler::~FontHandler()
{
}

FontID
FontHandler::AddFont(
	const char* path)
{
	const FontID id = myFontIDKeeper.FetchFreeID();
	if (BAD_ID(id))
	{
		LOG("no more font ids");
		return id;
	}

	FT_Face face;

	auto result = FT_New_Face(FTLibrary, path, 0, &face);
	if (result)
	{
		myFontIDKeeper.ReturnID(id);
		return FontID(INVALID_ID);
	}

	std::vector<char> fontData;
	int res = 512;
	int numLayers = 0;
	for (char c = FirstFontGlyph; c <= LastFontGlyph; ++c)
	{
		auto rawGlyph = DrawGlyph(face, res, c);
		myFontGlyphStrides[int(id)][c - FirstFontGlyph] = rawGlyph.metrics;
		fontData.insert(fontData.end(), rawGlyph.bmp.begin(), rawGlyph.bmp.end());
		numLayers++;
	}

	VkImageView fontView{};
	std::tie(result, fontView) = theirImageAllocator.RequestImageArray(fontData,
																	   numLayers,
																	   res,
																	   res,
																	   NUM_MIPS(res),
																	   myOwners.data(),
																	   myOwners.size(),
																	   VK_FORMAT_R8_UNORM);


	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo.imageView = fontView;
	imgInfo.sampler = mySampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;

	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.dstSet = myFontArraySet;
	write.dstBinding = 0;
	write.dstArrayElement = uint32_t(id);
	write.pImageInfo = &imgInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	return id;
}

VkDescriptorSetLayout
FontHandler::GetFontArraySetLayout() const
{
	return myFontArraySetLayout;
}

VkDescriptorSet
FontHandler::GetFontArraySet() const
{
	return myFontArraySet;
}

void
FontHandler::BindFonts(
	VkCommandBuffer		commandBuffer,
	VkPipelineLayout	pipelineLayout,
	uint32_t			setIndex,
	VkPipelineBindPoint bindPoint) const
{
	vkCmdBindDescriptorSets(commandBuffer,
							bindPoint,
							pipelineLayout,
							setIndex,
							1,
							&myFontArraySet,
							0,
							nullptr);
}

ImageAllocator& 
FontHandler::GetImageAllocator() const
{
	return theirImageAllocator;
}

GlyphMetrics
FontHandler::GetGlyphMetrics(
	FontID		id, 
	uint32_t	glyphIndex) const
{
	return myFontGlyphStrides[int(id)][glyphIndex];
}

RawGlyph
FontHandler::DrawGlyph(
	FT_Face		fontFace,
	int			resolution,
	char		glyph)
{
	auto result = FT_Set_Pixel_Sizes(fontFace, resolution, resolution);
	if (result)
	{
		return {};
	}

	// RAW GLYPH

	auto slot = fontFace->glyph;

	std::vector<char> rawImg;
	rawImg.resize(resolution * resolution);

	auto charIndex = FT_Get_Char_Index(fontFace, glyph);

	result = FT_Load_Glyph(fontFace, charIndex, FT_LOAD_DEFAULT);
	if (result)
	{
		return {};
	}
	result = FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);
	if (result)
	{
		return {};
	}

	auto* const bitmap = &slot->bitmap;

	const int advance = slot->metrics.horiAdvance >> 6;
	const int bearingX = slot->metrics.horiBearingX >> 6;
	const int bearingY = slot->metrics.horiBearingY >> 6;
	
	const int stride = advance - bearingX;
	const int yOffset = bitmap->rows - bearingY;
	float strideF = float(stride) / float(resolution);
	float yOffsetF = float(yOffset) / float(resolution);

	const int yMax = std::min(int(bitmap->rows), resolution);
	const int xMax = int(bitmap->width);

	for (int y = 0; y < yMax; y++)
	{
		for (int x = 0; x < xMax; x++)
		{
			int iy = resolution - y - 1;
			rawImg[iy * resolution + x] = bitmap->buffer[(yMax - y - 1) * bitmap->width + x];
		}
	}

	GlyphMetrics metrics;
	metrics.xStride = strideF;
	metrics.yOffset = -abs(yOffsetF);
	return {rawImg, metrics};
}
