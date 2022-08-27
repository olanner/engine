#include "pch.h"
#include "MeshHandle.h"

#include "Reflex.h"
#include "HandlesInternal.h"

MeshID rflx::MeshHandle::GetID() const
{
	return myMeshID;
}

void
rflx::MeshHandle::Load() const
{
	gMeshHandler->LoadMesh(myMeshID, gAllocationSubmissionIDs[int(theirReflex.GetThreadID())], myPath);
	gAccStructHandler->LoadGeometryStructure(myGeoID, gAllocationSubmissionIDs[int(theirReflex.GetThreadID())], (*gMeshHandler)[myMeshID].geo);
}

void rflx::MeshHandle::Unload() const
{
	gMeshHandler->UnloadMesh(myMeshID);
	gAccStructHandler->UnloadGeometryStructure(myGeoID);
}

rflx::MeshHandle::MeshHandle(
	Reflex& reflex,
	MeshID id,
	GeoStructID	geoID,
	std::string path)
	: theirReflex(reflex)
	, myMeshID(id)
	, myGeoID(geoID)
	, myPath(std::move(path))
{
}
