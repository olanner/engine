#include "pch.h"
#include "ImageHandle.h"

ImageID ImageHandle::GetID() const
{
    return myID;
}

ImageHandle::ImageHandle(ImageID id)
: myID(id)
{
}
