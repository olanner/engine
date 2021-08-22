#include "pch.h"
#include "MeshHandle.h"

MeshID rflx::MeshHandle::GetID() const
{
	return myID;
}

rflx::MeshHandle::MeshHandle(MeshID id)
: myID(id)
{
}
