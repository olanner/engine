#include "pch.h"
#include "ImageHandle.h"

#include "HandlesInternal.h"
#include "Reflex.h"

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

void
rflx::ImageHandle::Load() const
{
	const int threadID = int(theirReflex.GetThreadID());
	gImageHandler->LoadImage2D(myID, gAllocationSubmissionIDs[threadID], myPath);
}

void
rflx::ImageHandle::Unload() const
{
	gImageHandler->UnloadImage2D(myID);
}

rflx::ImageHandle::ImageHandle(
	Reflex&		reflex,
	ImageID		id,
	std::string path)
	: theirReflex(reflex)
	, myID(id)
	, myPath(path)
{
}
