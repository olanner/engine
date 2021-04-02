
#pragma once

enum class Channel : char
{
	R, G, B, A
};

struct ImageFormat
{
	uint32_t numChannels;
	size_t channelByteSize;
	Channel channelSwizzle[4];
};

struct RawImage
{
	uint32_t width;
	uint32_t height;
	std::vector<std::vector<char>> images;
};

std::tuple<ImageFormat, RawImage> ReadImage(const char *);