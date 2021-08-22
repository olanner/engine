
#pragma once

#include "Scene/SceneGlobals.h"
#include "Image/ImageHandler.h"
#include "Mesh/MeshHandler.h"
#include "Text/FontHandler.h"
#include "Image/CubeFilterer.h"
#include "Ray Tracing/AccelerationStructureHandler.h"


//CORE
inline std::shared_ptr<SceneGlobals>					gSceneGlobals;
inline std::shared_ptr<MeshHandler>						gMeshHandler;
inline std::shared_ptr<ImageHandler>					gImageHandler;
inline std::shared_ptr<FontHandler>						gFontHandler;
inline std::shared_ptr<CubeFilterer>					gCubeFilterer;
inline std::shared_ptr<AccelerationStructureHandler>	gAccStructHandler;
