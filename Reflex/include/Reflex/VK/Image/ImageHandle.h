
#pragma once

class ImageHandler;
namespace rflx
{	
	class ImageHandle;
	ImageHandle CreateImage(const char* path, Vec2f tiling = { 1,1 });
	ImageHandle CreateImage(std::vector<PixelValue>&& pixelData, Vec2f tiling = { 1,1 });

	class ImageHandle
	{
		friend ImageHandle CreateImage(const char*, Vec2f);
		friend ImageHandle CreateImage(std::vector<PixelValue>&&, Vec2f);

	public:
		ImageID		GetID() const;
		Vec2f		GetDimensions() const;
		uint32_t	GetLayers() const;

	private:
					ImageHandle(ImageID	id);

		ImageID							myID;

	};
}