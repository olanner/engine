
#pragma once

enum class Channel : char
{
	R, G, B, A
};

struct ImageFormat
{
	uint32_t numChannels = 0;
	size_t channelByteSize = 0;
	Channel channelSwizzle[4] = {};
};

struct RawImage
{
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<std::vector<char>> images;
};

std::tuple<ImageFormat, RawImage> ReadImage(const char *);