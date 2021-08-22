#include "pch.h"
#include "ImageHandle.h"

#include "Reflex/VK/HandlesInternal.h"

ImageID rflx::ImageHandle::GetID() const
{
	return myID;
}

Vec2f
rflx::ImageHandle::GetDimensions() const
{
	return (*gImageHandler)[myID].dim;
}

uint32_t
rflx::ImageHandle::GetLayers() const
{
	return (*gImageHandler)[myID].layers;
}

rflx::ImageHandle::ImageHandle(ImageID id)
: myID(id)
{
}
