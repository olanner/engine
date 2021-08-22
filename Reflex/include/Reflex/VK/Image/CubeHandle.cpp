#include "pch.h"
#include "CubeHandle.h"

#include "Reflex/VK/HandlesInternal.h"

CubeID rflx::CubeHandle::GetID() const
{
	return myID;
}

float rflx::CubeHandle::GetDim() const
{
	return (*gImageHandler)[myID].dim;
}

void
rflx::CubeHandle::SetAsSkybox() const
{
	gSceneGlobals->SetSkybox(myID);
}

rflx::CubeHandle::CubeHandle(CubeID	id)
: myID(id)
{
}
