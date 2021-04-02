
#pragma once


class ImageHandle;
namespace rflx
{
	ImageHandle LoadImage2D(const char*);
	ImageHandle LoadImage2D(std::vector<PixelValue>&&);
}

class ImageHandle
{
	friend ImageHandle rflx::LoadImage2D(const char*);
	friend ImageHandle rflx::LoadImage2D(std::vector<PixelValue>&&);

public:
	ImageID		GetID() const
	{
		return myID;
	}

private:
				ImageHandle(ImageID id);

	ImageID		myID;

};