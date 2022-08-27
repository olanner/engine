
#pragma once

struct GlyphMetrics
{
	float				xStride;
	float				yOffset;
};

struct Font
{
	ImageID										imgArrID;
	std::array<GlyphMetrics, NumGlyphsPerFont>	metrics;
};

struct RawGlyph
{
	std::vector<uint8_t>	bmp;
	GlyphMetrics			metrics;
};

class FontHandler
{
public:
												FontHandler(
													class VulkanFramework&	vulkanFramework,
													class ImageHandler&		imageHandler,
													QueueFamilyIndex*		firstOwner,
													uint32_t				numOwners);
												~FontHandler();

	FontID										AddFont(
													AllocationSubmissionID allocSubID,
													const char* path);
	
	
	GlyphMetrics								GetGlyphMetrics(
													FontID		id,
													uint32_t	glyphIndex) const;

	ImageHandler&								GetImageHandler();

	Font										operator[](FontID id);

private:
	static RawGlyph								DrawGlyph(
													struct FT_FaceRec_* fontFace,
													int					resolution,
													char				glyph);
	
	VulkanFramework&							theirVulkanFramework;
	ImageHandler&								theirImageHandler;

	std::array<Font, MaxNumFonts>				myFonts;
	
	IDKeeper<FontID>							myFontIDKeeper;

	neat::static_vector<QueueFamilyIndex, 16>	myOwners;


};