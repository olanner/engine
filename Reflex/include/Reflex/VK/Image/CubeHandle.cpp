#include "pch.h"
#include "CubeHandle.h"

#include "Reflex/VK/Scene/SceneGlobals.h"

CubeID CubeHandle::GetID() const
{
    return myID;
}

void
CubeHandle::SetAsSkybox() const
{
	theirSceneGlobals->SetSkybox(myID);
}

CubeHandle::CubeHandle(CubeID id) :
	myID(id)
{
}
