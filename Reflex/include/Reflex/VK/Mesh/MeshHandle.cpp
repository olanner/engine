#include "pch.h"
#include "MeshHandle.h"

#include "Reflex/Reflex.h"
#include "Reflex/VK/HandlesInternal.h"

MeshID rflx::MeshHandle::GetID() const
{
	return myID;
}

void
rflx::MeshHandle::Load() const
{
	gMeshHandler->LoadMesh(myID, gAllocationSubmissions[int(theirReflex.GetThreadID())], myPath.c_str());
}

void rflx::MeshHandle::Unload() const
{
	gMeshHandler->UnloadMesh(myID);
}

rflx::MeshHandle::MeshHandle(
	Reflex& reflex,
	MeshID id, 
	std::string path)
	: theirReflex(reflex)
	, myID(id)
	, myPath(std::move(path))
{
}
