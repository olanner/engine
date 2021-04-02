
#pragma once
#include <iosfwd>
#include <vector>

#include "Reflex/VK/Image/ImageHandle.h"

class MeshHandle;
namespace rflx
{
	MeshHandle LoadMesh(const char*, std::vector<ImageHandle>&&);
}

class MeshHandle
{
	friend MeshHandle rflx::LoadMesh(const char*, std::vector<ImageHandle>&&);

public:
	MeshID	GetID() const
	{
		return myID;
	}

private:
			MeshHandle(MeshID id);

	MeshID	 myID;

};