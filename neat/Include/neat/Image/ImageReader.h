
#pragma once

namespace neat
{
	enum class ImageSwizzle
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
		ImageSwizzle swizzle = ImageSwizzle::Unknown;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t layers = 0;
		uint32_t bitDepth = 0;
		uint32_t colorDepth = 0;
		uint32_t alphaDepth = 0;
		std::vector<uint8_t> pixelData;
		ImageError error = ImageError::GenericError;
	};

	Image ReadImage(
			const char* path, 
			bool		alphaPadding = true);
}