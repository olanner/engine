
#pragma once

#include "RFVK/Scene/SceneGlobals.h"
#include "RFVK/Image/ImageHandler.h"
#include "RFVK/Mesh/MeshHandler.h"
#include "RFVK/Text/FontHandler.h"
#include "RFVK/Image/CubeFilterer.h"
#include "RFVK/Ray Tracing/AccelerationStructureHandler.h"
#include "RFVK/Memory/AllocatorBase.h"

//CORE
inline std::shared_ptr<SceneGlobals>					gSceneGlobals;
inline std::shared_ptr<MeshHandler>						gMeshHandler;
inline std::shared_ptr<ImageHandler>					gImageHandler;
inline std::shared_ptr<FontHandler>						gFontHandler;
inline std::shared_ptr<CubeFilterer>					gCubeFilterer;
inline std::shared_ptr<AccelerationStructureHandler>	gAccStructHandler;

inline std::array<AllocationSubmission, neat::MaxThreadID> gAllocationSubmissions;