
#pragma once

struct GlyphMetrics
{
	float				xStride;
	float				yOffset;
};

struct RawGlyph
{
	std::vector<char>	bmp;
	GlyphMetrics		metrics;
};

class FontHandler
{
public:
												FontHandler(
													class VulkanFramework&	vulkanFramework,
													class ImageAllocator&	imageAllocator,
													QueueFamilyIndex*		firstOwner,
													uint32_t				numOwners);
												~FontHandler();

	FontID										AddFont(const char* path);
	
	VkDescriptorSetLayout						GetFontArraySetLayout() const;
	VkDescriptorSet								GetFontArraySet() const;

	void										BindFonts(
													VkCommandBuffer		commandBuffer,
													VkPipelineLayout	pipelineLayout,
													uint32_t			setIndex,
													VkPipelineBindPoint	bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS) const;

	ImageAllocator&								GetImageAllocator() const;
	GlyphMetrics								GetGlyphMetrics(
													FontID		id,
													uint32_t	glyphIndex) const;

private:
	static RawGlyph								DrawGlyph(
													FT_Face	fontFace,
													int		resolution,
													char	glyph);
	
	VulkanFramework&							theirVulkanFramework;
	ImageAllocator&								theirImageAllocator;

	VkSampler									mySampler;
	VkDescriptorPool							myDescriptorPool;
	VkDescriptorSetLayout						myFontArraySetLayout;
	VkDescriptorSet								myFontArraySet;

	std::array<VkImageView, MaxNumFonts>		myFonts;
	std::array<std::array<GlyphMetrics, NumGlyphsPerFont>, MaxNumFonts>
												myFontGlyphStrides;
	IDKeeper<FontID>							myFontIDKeeper;

	neat::static_vector<QueueFamilyIndex, 16>	myOwners;


};