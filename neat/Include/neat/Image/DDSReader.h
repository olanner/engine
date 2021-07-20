#pragma once

struct RawDDS
{
	uint32_t width;
	uint32_t height;
	uint32_t numMipMaps;
	uint32_t numLayers;
	uint32_t maskA;
	uint32_t maskR;
	uint32_t maskG;
	uint32_t maskB;
	uint32_t pixelDepth;
	std::vector<std::vector<std::vector<char>>> images;
	std::vector<char> imagesInline;
};

RawDDS ReadDDS(const char* path);