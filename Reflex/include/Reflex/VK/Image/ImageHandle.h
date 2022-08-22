
#pragma once

class ImageHandler;
namespace rflx
{
	class Reflex;
	class ImageHandle
	{
		friend Reflex;

	public:
		ImageID		GetID() const;
		Vec2f		GetDimensions() const;
		uint32_t	GetLayers() const;

		void		Load() const;
		void		Unload() const;

	private:
					ImageHandle(
						Reflex&		reflex,
						ImageID		id,
						std::string path);

		Reflex&		theirReflex;
		ImageID		myID;
		std::string	myPath;

	};
}