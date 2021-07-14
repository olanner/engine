
#pragma once
#include <iosfwd>
#include <vector>

#include "Reflex/VK/Image/ImageHandle.h"

class MeshHandle;
namespace rflx
{
	MeshHandle CreateMesh(
				const char* path, 
				std::vector<ImageHandle>&& imgHandles = {});
}

class MeshHandle
{
	friend MeshHandle rflx::CreateMesh(const char*, std::vector<ImageHandle>&&);

public:
	MeshID	GetID() const;

private:
			MeshHandle(MeshID id);

	MeshID	 myID;

};