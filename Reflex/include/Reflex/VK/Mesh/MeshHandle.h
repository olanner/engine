
#pragma once
#include <iosfwd>
#include <vector>

namespace rflx
{
	class MeshHandle;
	MeshHandle CreateMesh(
		const char* path,
		std::vector<class ImageHandle>&& imgHandles = {});

	class MeshHandle
	{
		friend MeshHandle rflx::CreateMesh(const char*, std::vector<ImageHandle>&&);

	public:
		MeshID	GetID() const;

	private:
				MeshHandle(MeshID id);

		MeshID	myID;

	};
}