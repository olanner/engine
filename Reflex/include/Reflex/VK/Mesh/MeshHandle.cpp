#include "pch.h"
#include "MeshHandle.h"

MeshID MeshHandle::GetID() const
{
	return myID;
}

MeshHandle::MeshHandle(MeshID id)
: myID(id)
{
}
