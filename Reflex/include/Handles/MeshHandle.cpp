#include "pch.h"
#include "MeshHandle.h"

#include "Reflex.h"
#include "HandlesInternal.h"

MeshID rflx::MeshHandle::GetID() const
{
	return myID;
}

void
rflx::MeshHandle::Load() const
{
	gMeshHandler->LoadMesh(myID, gAllocationSubmissions[int(theirReflex.GetThreadID())], myPath.c_str());
	gAccStructHandler->LoadGeometryStructure(myID, gAllocationSubmissions[int(theirReflex.GetThreadID())], (*gMeshHandler)[myID].geo);
}

void rflx::MeshHandle::Unload() const
{
	gMeshHandler->UnloadMesh(myID);
	gAccStructHandler->UnloadGeometryStructure(myID);
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
