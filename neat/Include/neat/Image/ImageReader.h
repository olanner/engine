
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
	inline std::string ImageErrorStr(ImageError error)
	{
		switch (error) {
		case ImageError::None: 
			return "None";
		case ImageError::FileReadError: 
			return "File Read Error";
		case ImageError::UnsupportedFileFormat: 
			return "Unsupported File Format";
		case ImageError::UnsupportedImageFormat: 
			return "Unsupported Image Format";
		case ImageError::GenericError:
			return "Generic Error"; }
		return "Generic Error";
	}

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