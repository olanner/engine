#pragma once

struct RawDDS
{
	uint32_t width;
	uint32_t height;
	uint32_t numMipMaps;
	uint32_t numLayers;
	std::vector<std::vector<std::vector<char>>> images;
	std::vector<char> imagesInline;
};

RawDDS ReadDDS(const char* path);