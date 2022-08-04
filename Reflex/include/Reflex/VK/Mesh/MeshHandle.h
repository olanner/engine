
#pragma once
#include <iosfwd>
#include <vector>

namespace rflx
{
	class MeshHandle
	{
		friend class Reflex;

	public:
		MeshID	GetID() const;

	private:
				MeshHandle(MeshID id);

		MeshID	myID;

	};
}