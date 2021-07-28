#include "pch.h"
#include "FontHandler.h"


#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/ImageAllocator.h"
#include "Reflex/VK/Image/ImageHandler.h"

#ifdef _DEBUG
#pragma comment(lib, "freetype-d.lib")
#else
#pragma comment(lib, "freetype.lib")
#endif

FT_Library FTLibrary;

FontHandler::FontHandler(
	VulkanFramework& vulkanFramework,
	ImageHandler& imageHandler,
	QueueFamilyIndex* firstOwner,
	uint32_t numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirImageHandler(imageHandler)
	, myFontIDKeeper(MaxNumFonts)
{
	myOwners.resize(numOwners);
	for (uint32_t ownerIndex = 0; ownerIndex < numOwners; ++ownerIndex)
	{
		myOwners[ownerIndex] = firstOwner[ownerIndex];
	}

	auto result = FT_Init_FreeType(&FTLibrary);
	assert(!result && "failed initializing free type");

	AddFont("open_sans_reg.ttf");

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

	Font& font = myFonts[int(id)];

	FT_Face face;

	auto result = FT_New_Face(FTLibrary, path, 0, &face);
	if (result)
	{
		myFontIDKeeper.ReturnID(id);
		return FontID(INVALID_ID);
	}

	std::vector<uint8_t> fontData;
	int res = 512;
	int numLayers = 0;
	for (char c = FirstFontGlyph; c <= LastFontGlyph; ++c)
	{
		auto rawGlyph = DrawGlyph(face, res, c);
		font.metrics[c - FirstFontGlyph] = rawGlyph.metrics;
		fontData.insert(fontData.end(), rawGlyph.bmp.begin(), rawGlyph.bmp.end());
		numLayers++;
	}

	font.imgArrID = theirImageHandler.AddImage2D(std::move(fontData), {res,res}, VK_FORMAT_R8G8B8A8_UNORM, 4);
	
	return id;
}


GlyphMetrics
FontHandler::GetGlyphMetrics(
	FontID		id, 
	uint32_t	glyphIndex) const
{
	return myFonts[int(id)].metrics[glyphIndex];
}

ImageHandler& 
FontHandler::GetImageHandler()
{
	return theirImageHandler;
}

Font
FontHandler::operator[](
	FontID id)
{
	return myFonts[int(id)];
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

	std::vector<PixelValue> rawPixels;
	rawPixels.resize(resolution * resolution, {255,255,255,0});

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
			rawPixels[iy * resolution + x].a = bitmap->buffer[(yMax - y - 1) * bitmap->width + x];
		}
	}

	GlyphMetrics metrics;
	metrics.xStride = strideF;
	metrics.yOffset = -abs(yOffsetF);

	std::vector<uint8_t> rawImg;
	rawImg.resize(rawPixels.size() * 4);
	memcpy(rawImg.data(), rawPixels.data(), rawImg.size());
	return { rawImg, metrics};
}
