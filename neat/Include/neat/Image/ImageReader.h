
#pragma once

namespace neat
{
	enum class ChannelSwizzle
	{
		Unknown,
		R,
		RGBA,
		RGB,
		ABGR,
		BGRA,
		BGR,
	};

	enum class ImageFileFormat
	{
		Unknown,
		DDS,
		Targa

	};

	enum class ImageError
	{
		None,
		FileReadError,
		UnsupportedFileFormat,
		UnsupportedImageFormat,
		GenericError
	};

	struct Image
	{
		ImageFileFormat fileFormat = ImageFileFormat::Unknown;
		ChannelSwizzle swizzle = ChannelSwizzle::Unknown;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t layers = 0;
		uint32_t pixelDepth = 0;
		std::vector<char> pixelData;
		ImageError error = ImageError::GenericError;
	};

	Image ReadImage(const char* path);
}