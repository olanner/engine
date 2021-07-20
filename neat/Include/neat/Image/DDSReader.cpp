#include "pch.h"
#include "DDSReader.h"

#include <fstream>

#include <d3d11.h>

struct DDS_PIXELFORMAT
{
	uint32_t dwSize;
	uint32_t dwFlags;
	uint32_t dwFourCC;
	uint32_t dwRGBBitCount;
	uint32_t dwRBitMask;
	uint32_t dwGBitMask;
	uint32_t dwBBitMask;
	uint32_t dwABitMask;
};

typedef struct
{
	uint32_t           size;
	uint32_t           flags;
	uint32_t           height;
	uint32_t           width;
	uint32_t           pitchOrLinearSize;
	uint32_t           depth;
	uint32_t           mipMapCount;
	uint32_t           dwReserved1[11];
	DDS_PIXELFORMAT pixelFormat;
	uint32_t           dwCaps;
	uint32_t           dwCaps2;
	uint32_t           dwCaps3;
	uint32_t           dwCaps4;
	uint32_t           dwReserved2;
} DDS_HEADER;

typedef struct
{
	DXGI_FORMAT              dxgiFormat;
	D3D10_RESOURCE_DIMENSION resourceDimension;
	uint32_t                     miscFlag;
	uint32_t                     arraySize;
	uint32_t                     miscFlags2;
} DDS_HEADER_DXT10;

struct DDS
{
	uint32_t magic;
	DDS_HEADER header;
	byte data[];
};

struct DDSDXT10
{
	uint32_t magic;
	DDS_HEADER header;
	DDS_HEADER_DXT10 dxt10header;
	byte data[];
};
#define DDSCAPS2_CUBEMAP 0x200

RawDDS ReadDDS(const char* path)
{
	RawDDS ret;
	std::vector<char> rawData;
	std::ifstream in;
	in.open(path, std::ios::binary | std::ios::ate);
	if (!in.good())
	{
		return {};
	}

	size_t fileSize = in.tellg();
	rawData.resize(fileSize);
	in.seekg(0, std::ios::beg);
	in.read(rawData.data(), fileSize);

	DDS* dds = (DDS*)rawData.data();
	ret.width = dds->header.width;
	ret.height = dds->header.height;
	ret.numLayers = 1;
	ret.numMipMaps = dds->header.mipMapCount;

	if (dds->header.dwCaps2 & DDSCAPS2_CUBEMAP)
	{
		ret.numLayers = 6;
	}

	ret.maskA = dds->header.pixelFormat.dwABitMask;
	ret.maskR = dds->header.pixelFormat.dwRBitMask;
	ret.maskG = dds->header.pixelFormat.dwGBitMask;
	ret.maskB = dds->header.pixelFormat.dwBBitMask;
	ret.pixelDepth = dds->header.pixelFormat.dwRGBBitCount;

	ret.images.resize(ret.numLayers);
	if (strcmp((char*)&dds->header.pixelFormat.dwFourCC, "DX10"))
	{
		int offset = 0;
		for (int i = 0; i < ret.numLayers; ++i)
		{
			std::vector<std::vector<char>> images;
			images.resize(dds->header.mipMapCount);

			for (int i = 0; i < dds->header.mipMapCount; ++i)
			{
				char* data = (char*)(dds->data + offset);
				images[i].resize((dds->header.width) / pow(2, i) * (dds->header.height) / pow(2, i) * 4);
				memcpy(images[i].data(), data, images[i].size());
				offset += images[i].size();
			}
			ret.images[i] = images;
		}
	}

	for (auto& layer : ret.images)
	{
		ret.imagesInline.insert(ret.imagesInline.end(), layer[0].begin(), layer[0].end());
	}

	return ret;
}
