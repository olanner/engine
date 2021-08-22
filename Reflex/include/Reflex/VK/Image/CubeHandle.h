
#pragma once

class ImageHandler;
class SceneGlobals;

namespace rflx
{
	class CubeHandle;
	CubeHandle CreateImageCube(const char* path);

	class CubeHandle
	{
		friend CubeHandle rflx::CreateImageCube(const char*);
		
	public:
		CubeID	GetID() const;
		float	GetDim() const;
		void	SetAsSkybox() const;

	private:
				CubeHandle(CubeID id);

		CubeID	myID;

	};
}