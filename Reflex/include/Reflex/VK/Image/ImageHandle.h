
#pragma once

class ImageHandler;
namespace rflx
{	
	class ImageHandle
	{
		friend class Reflex;

	public:
		ImageID		GetID() const;
		Vec2f		GetDimensions() const;
		uint32_t	GetLayers() const;

	private:
					ImageHandle(ImageID	id);

		ImageID							myID;

	};
}