
#pragma once

class AllocationSubmission;
namespace rflx
{
	class Reflex;
	class MeshHandle
	{
		friend class Reflex;

	public:
		MeshID	GetID() const;
		void	Load() const;
		void	Unload() const;

	private:
				MeshHandle(
					Reflex&		reflex,
					MeshID		id,
					GeoStructID	geoID,
					std::string path);

		Reflex&			theirReflex;
		MeshID			myMeshID;
		GeoStructID		myGeoID;
		std::string		myPath;

	};
}