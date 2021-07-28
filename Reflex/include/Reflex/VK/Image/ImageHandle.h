
#pragma once


class ImageHandle;
namespace rflx
{
	ImageHandle CreateImage(const char* path, Vec2f tiling = {1,1});
	ImageHandle CreateImage(std::vector<PixelValue>&& pixelData, Vec2f tiling = {1,1});
}

class ImageHandle
{
	friend ImageHandle rflx::CreateImage(const char*, Vec2f);
	friend ImageHandle rflx::CreateImage(std::vector<PixelValue>&&, Vec2f);

public:
	ImageID	GetID() const;

private:
			ImageHandle(ImageID id);

	ImageID	myID;

};