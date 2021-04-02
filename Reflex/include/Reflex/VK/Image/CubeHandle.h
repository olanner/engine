
#pragma once

class CubeHandle;
namespace rflx
{
	CubeHandle LoadImageCube(const char*);
}

class CubeHandle
{
	friend CubeHandle rflx::LoadImageCube(const char*);


public:
	CubeID					GetID() const
	{
		return myID;
	}

	void					SetAsSkybox() const;

private:
							CubeHandle(CubeID id);

	std::shared_ptr<class SceneGlobals>		theirSceneGlobals;
	CubeID					myID;

};