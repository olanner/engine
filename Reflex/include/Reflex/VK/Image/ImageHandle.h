
#pragma once


class ImageHandle;
namespace rflx
{
	ImageHandle CreateImage(const char* path);
	ImageHandle CreateImage(std::vector<PixelValue>&& pixelData);
}

class ImageHandle
{
	friend ImageHandle rflx::CreateImage(const char*);
	friend ImageHandle rflx::CreateImage(std::vector<PixelValue>&&);

public:
	ImageID	GetID() const;

private:
			ImageHandle(ImageID id);

	ImageID	myID;

};