
#pragma once

class ImageHandler;
class SceneGlobals;

namespace rflx
{
	class CubeHandle
	{
		friend class Reflex;
	public:
		CubeID	GetID() const;
		float	GetDim() const;
		void	SetAsSkybox() const;

	private:
				CubeHandle(CubeID id);

		CubeID	myID;

	};
}