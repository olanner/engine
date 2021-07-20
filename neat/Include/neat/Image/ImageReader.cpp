#include "pch.h"
#include "ImageReader.h"

#include "DDSReader.h"
#include "libtga/tga.h"
namespace neat
{
void OpenTGA(Image & outImg, const char* path);
void OpenDDS(Image & outImg, const char* path);

Image
ReadImage(const char* path)
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
		ret.error = ImageError::FileReadError;
	}
	
	return ret;
}

#define X 0xff000000
#define Y 0x00ff0000
#define Z 0x0000ff00
#define W 0x000000ff

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
		outImg.swizzle = ChannelSwizzle::RGBA;
	}
	else 
	if (tgaData.flags & TGA_BGR
		&& tga->hdr.alpha == 8)
	{
		outImg.swizzle = ChannelSwizzle::BGRA;
	}
	else 
	if (tgaData.flags & TGA_RGB
		&& tga->hdr.alpha == 0)
	{
		outImg.swizzle = ChannelSwizzle::RGB;
	}
	else 
	if (tgaData.flags & TGA_BGR
		&& tga->hdr.alpha == 0)
	{
		outImg.swizzle = ChannelSwizzle::BGR;
	}
	else
	{
		outImg.error = ImageError::UnsupportedImageFormat;
		return;
	}

	outImg.width = tga->hdr.width;
	outImg.height = tga->hdr.height;
	outImg.layers = 1;
	outImg.pixelDepth = tga->hdr.depth;

	size_t dataSize = outImg.width * outImg.height * (outImg.pixelDepth / 8);
	outImg.pixelData.resize(dataSize);
	memcpy(outImg.pixelData.data(), tgaData.img_data, dataSize);
	
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
		outImg.swizzle = ChannelSwizzle::R;
	}
	else 
	if (rawDDS.maskR == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskB == Z
		&& rawDDS.maskA == W)
	{
		outImg.swizzle = ChannelSwizzle::RGBA;
	}
	else
	if (rawDDS.maskR == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskB == Z
		&& rawDDS.maskA == 0)
	{
		outImg.swizzle = ChannelSwizzle::RGB;
	}
	else 
	if (rawDDS.maskA == X
		&& rawDDS.maskB == Y
		&& rawDDS.maskG == Z
		&& rawDDS.maskR == W)
	{
		outImg.swizzle = ChannelSwizzle::ABGR;
	}
	else
	if (rawDDS.maskB == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskR == Z
		&& rawDDS.maskA == W)
	{
		outImg.swizzle = ChannelSwizzle::BGRA;
	}
	else 
	if (rawDDS.maskB == X
		&& rawDDS.maskG == Y
		&& rawDDS.maskR == Z
		&& rawDDS.maskA == 0)
	{
		outImg.swizzle = ChannelSwizzle::BGR;
	}
	else
	{
		outImg.error = ImageError::UnsupportedImageFormat;
		return;
	}

	outImg.width = rawDDS.width;
	outImg.height = rawDDS.height;
	outImg.layers = rawDDS.numLayers;
	outImg.pixelDepth = rawDDS.pixelDepth;
	outImg.pixelData = std::move(rawDDS.imagesInline);

	outImg.error = ImageError::None;
}
}