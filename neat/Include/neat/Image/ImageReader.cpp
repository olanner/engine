#include "pch.h"
#include "ImageReader.h"

#include "DDSReader.h"
#include "libtga/tga.h"
namespace neat
{
void OpenTGA(Image & outImg, const char* path);
void OpenDDS(Image & outImg, const char* path);

Image
ReadImage(
	const char* path,
	bool		alphaPadding)
{
	Image ret;
	if (strstr(path, ".tga"))
	{
		OpenTGA(ret, path);
	}
	else if (strstr(path, ".dds"))
	{
		OpenDDS(ret, path);
	}
	else
	{
		ret.error = ImageError::UnsupportedFileFormat;
		return ret;
	}

	if (alphaPadding)
	{
		if (ret.swizzle == ImageSwizzle::RGB 
			|| ret.swizzle == ImageSwizzle::BGR)
		{
			ret.alphaDepth = 8;
			ret.bitDepth += 8;
			for (int pix = ret.pixelData.size() - 1; pix >= 0; pix -= 3)
			{
				ret.pixelData.insert(ret.pixelData.begin() + pix + 1, 255);
			}
			if (ret.swizzle == ImageSwizzle::RGB)
			{
				ret.swizzle = ImageSwizzle::RGBA;
			}
			if (ret.swizzle == ImageSwizzle::BGR)
			{
				ret.swizzle = ImageSwizzle::BGRA;
			}
		}
	}
	
	return ret;
}

#define X 0x000000ff
#define Y 0x0000ff00
#define Z 0x00ff0000
#define W 0xff000000

void
OpenTGA(
	Image&	outImg, 
	const char*		path)
{
	outImg.fileFormat = ImageFileFormat::Targa;
	
	TGA* tga = TGAOpen(path, "r");
	if (!tga)
	{
		outImg.error = ImageError::FileReadError;
		return;
	}
	tga->error = nullptr;
	
	TGAData tgaData;
	int error = TGAReadImage(tga, &tgaData);
	if (error)
	{
		outImg.error = ImageError::FileReadError;
		return;
	}
	
	if (tgaData.flags & TGA_RGB
		&& tga->hdr.alpha == 8)
	{
		outImg.swizzle = ImageSwizzle::RGBA;
	}
	else 
	if (tgaData.flags & TGA_BGR
		&& tga->hdr.alpha == 8)
	{
		outImg.swizzle = ImageSwizzle::BGRA;
	}
	else 
	if (tgaData.flags & TGA_RGB
		&& tga->hdr.alpha == 0)
	{
		outImg.swizzle = ImageSwizzle::RGB;
	}
	else 
	if (tgaData.flags & TGA_BGR
		&& tga->hdr.alpha == 0)
	{
		outImg.swizzle = ImageSwizzle::BGR;
	}
	else
	{
		outImg.error = ImageError::UnsupportedImageFormat;
		return;
	}

	outImg.width = tga->hdr.width;
	outImg.height = tga->hdr.height;
	outImg.layers = 1;
	outImg.bitDepth = tga->hdr.depth;
	outImg.colorDepth = tga->hdr.depth - tga->hdr.alpha;
	outImg.alphaDepth = tga->hdr.alpha;

	for (int row = outImg.height - 1; row >= 0; --row)
	{
		const uint32_t bWidth = outImg.height * outImg.bitDepth / 8;
		const uint32_t index = row * bWidth;
		outImg.pixelData.insert(outImg.pixelData.end(), tgaData.img_data + index, tgaData.img_data + index + bWidth);
	}
	
	TGAClose(tga);
	outImg.error = ImageError::None;
}

void
OpenDDS(
	Image&	outImg, 
	const char*		path)
{
	auto rawDDS = ReadDDS(path);
	outImg.fileFormat = ImageFileFormat::DDS;

	if (rawDDS.pixelDepth == 8)
	{
		outImg.swizzle = ImageSwizzle::R;
	}
	else 
	if (rawDDS.maskR == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskB == Z
		&& rawDDS.maskA == W)
	{
		outImg.swizzle = ImageSwizzle::RGBA;
	}
	else
	if (rawDDS.maskR == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskB == Z
		&& rawDDS.maskA == 0)
	{
		outImg.swizzle = ImageSwizzle::RGB;
	}
	else 
	if (rawDDS.maskA == X
		&& rawDDS.maskB == Y
		&& rawDDS.maskG == Z
		&& rawDDS.maskR == W)
	{
		outImg.swizzle = ImageSwizzle::ABGR;
	}
	else
	if (rawDDS.maskB == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskR == Z
		&& rawDDS.maskA == W)
	{
		outImg.swizzle = ImageSwizzle::BGRA;
	}
	else 
	if (rawDDS.maskB == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskR == Z
		&& rawDDS.maskA == 0)
	{
		outImg.swizzle = ImageSwizzle::BGR;
	}
	else
	{
		outImg.error = ImageError::UnsupportedImageFormat;
		return;
	}

	outImg.width = rawDDS.width;
	outImg.height = rawDDS.height;
	outImg.layers = rawDDS.numLayers;
	outImg.bitDepth = rawDDS.pixelDepth;
	outImg.alphaDepth = !!rawDDS.maskA * 8;
	outImg.colorDepth = rawDDS.pixelDepth - outImg.alphaDepth;
	outImg.pixelData = std::move(rawDDS.imagesInline);

	outImg.error = ImageError::None;
}
}