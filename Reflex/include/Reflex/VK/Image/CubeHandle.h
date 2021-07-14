
#pragma once

class CubeHandle;
namespace rflx
{
	CubeHandle CreateImageCube(const char* path);
}

class CubeHandle
{
	friend CubeHandle rflx::CreateImageCube(const char*);


public:
	CubeID									GetID() const;
	void									SetAsSkybox() const;

private:
											CubeHandle(CubeID id);

	std::shared_ptr<class SceneGlobals>		theirSceneGlobals;
	CubeID									myID;

};