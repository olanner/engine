#include "pch.h"
#include "Reflex.h"
#include "Reflex/VK/HandlesInternal.h"
#include "Reflex/VK/VulkanImplementation.h"
#include "Reflex/VK/Mesh/MeshRenderer.h"
#include "Reflex/VK/Scene/SceneGlobals.h"
#include "VK/Image/CubeFilterer.h"
#include "VK/Image/ImageHandler.h"
#include "VK/Mesh/MeshHandler.h"
#include "VK/Mesh/MeshRenderCommand.h"
#include "VK/Ray Tracing/AccelerationStructureHandler.h"
#include "VK/Ray Tracing/RTMeshRenderer.h"
#include "VK/Sprite/SpriteRenderer.h"
#include "VK/Text/FontHandler.h"
#include "VK/Image/CubeHandle.h"
#include "VK/Image/ImageHandle.h"
#include "VK/Memory/AllocatorBase.h"

bool*		gUseRayTracing;

uint32_t				rflx::Reflex::ourUses = 0;
VulkanImplementation*	rflx::Reflex::ourVKImplementation = nullptr;

// FEATURES
std::shared_ptr<MeshRenderer>						gMeshRenderer;
std::shared_ptr<RTMeshRenderer>						gRTMeshRenderer;
std::shared_ptr<SpriteRenderer>						gSpriteRenderer;
std::array<AllocationSubmission, neat::MaxThreadID> gAllocationSubmissions;

VulkanFramework* gVulkanFramework;

rflx::Reflex::Reflex(neat::ThreadID threadID)
	: myThreadID(threadID)
{
	ourUses++;
}

rflx::Reflex::~Reflex()
{
	ourUses--;
	if (ourUses <= 0)
	{
		gSceneGlobals = nullptr;
		gMeshHandler = nullptr;
		gImageHandler = nullptr;
		gFontHandler = nullptr;
		gCubeFilterer = nullptr;
		gAccStructHandler = nullptr;

		gSpriteRenderer = nullptr;
		gMeshRenderer = nullptr;
		gRTMeshRenderer = nullptr;

		SAFE_DELETE(ourVKImplementation);
	}
}

bool
rflx::Reflex::Start(
	const WindowInfo&	windowInformation, 
	const char*			cmdArgs)
{
	my2DScaleRef.x = 1.f / float(windowInformation.width);
	my2DScaleRef.y = 1.f / float(windowInformation.height);
	
	if (ourVKImplementation)
	{
		ourVKImplementation->RegisterThread(myThreadID);
		return true;
	}
	
	assert(IsWindow((HWND)windowInformation.hWND) && "invalid window handle passed");

	bool useDebugLayers = false;
	if (cmdArgs)
	{
		useDebugLayers = std::string(cmdArgs).find("vkdebug") != std::string::npos;
	}

	ourVKImplementation = new VulkanImplementation;
	const auto result = ourVKImplementation->Initialize(windowInformation, useDebugLayers);
	if (result)
	{
		return false;
	}

	auto mr = std::make_shared<MeshRenderer>(ourVKImplementation->myVulkanFramework,
		*ourVKImplementation->myUniformHandler,
		*ourVKImplementation->myMeshHandler,
		*ourVKImplementation->myImageHandler,
		*ourVKImplementation->mySceneGlobals,
		*ourVKImplementation->myRenderPassFactory,
		ourVKImplementation->myPresQueueIndex);

	auto rtmr = std::make_shared<RTMeshRenderer>(ourVKImplementation->myVulkanFramework,
		*ourVKImplementation->myUniformHandler,
		*ourVKImplementation->myMeshHandler,
		*ourVKImplementation->myImageHandler,
		*ourVKImplementation->mySceneGlobals,
		*ourVKImplementation->myBufferAllocator,
		*ourVKImplementation->myAccStructHandler,
		ourVKImplementation->myCompQueueIndex,
		ourVKImplementation->myTransQueueIndex
		);

	QueueFamilyIndex indices[]{ ourVKImplementation->myPresQueueIndex, ourVKImplementation->myTransQueueIndex };
	auto tr = std::make_shared<SpriteRenderer>(ourVKImplementation->myVulkanFramework,
		*ourVKImplementation->mySceneGlobals,
		*ourVKImplementation->myImageHandler,
		*ourVKImplementation->myRenderPassFactory,
		*ourVKImplementation->myUniformHandler,
		indices,
		ARRAYSIZE(indices),
		ourVKImplementation->myPresQueueIndex);

	//myVKImplementation->RegisterWorkerSystem(rtmr,
	//	VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,
	//	VK_QUEUE_COMPUTE_BIT);

	ourVKImplementation->RegisterWorkerSystem(mr, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);
	ourVKImplementation->RegisterWorkerSystem(tr, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);

	ourVKImplementation->LockWorkerSystems();

	ourVKImplementation->RegisterThread(myThreadID);
	if (BAD_ID(myThreadID))
	{
		return false;
	}

	gMeshRenderer = mr;
	gSceneGlobals = ourVKImplementation->mySceneGlobals;
	gMeshHandler = ourVKImplementation->myMeshHandler;
	gAccStructHandler = ourVKImplementation->myAccStructHandler;
	gRTMeshRenderer = rtmr;
	gImageHandler = ourVKImplementation->myImageHandler;
	gCubeFilterer = ourVKImplementation->myCubeFilterer;
	gSpriteRenderer = tr;
	gFontHandler = ourVKImplementation->myFontHandler;

	gUseRayTracing = &ourVKImplementation->myUseRayTracing;

	gVulkanFramework = &ourVKImplementation->myVulkanFramework;
	return true;
}

void
rflx::Reflex::BeginFrame()
{
	ourVKImplementation->BeginFrame();
}

void
rflx::Reflex::Submit()
{
	ourVKImplementation->Submit();
}

void
rflx::Reflex::EndFrame()
{
	ourVKImplementation->EndFrame();
}

rflx::MeshHandle
rflx::Reflex::CreateMesh(
	const char* path,
	std::vector<ImageHandle>&& imgHandles)
{
	std::vector<ImageID> imgIDs;
	for (auto& handle : imgHandles)
	{
		imgIDs.emplace_back(handle.GetID());
	}

	MeshID id = gMeshHandler->AddMesh(gAllocationSubmissions[int(myThreadID)], path, std::move(imgIDs));
	MeshHandle ret(id);

	//if ( *gUseRayTracing )
	//{
	//Mesh mesh = (*gMeshHandler)[id];
	//assert(!gAccStructHandler->AddGeometryStructure(gAllocationSubmissions[int(myThreadID)], id, &mesh, 1) && "failed creating geo structure");
	//}

	return ret;
}

rflx::ImageHandle
rflx::Reflex::CreateImage(
	const char* path,
	Vec2f		tiling) const
{
	ImageID id = ImageID(INVALID_ID);// = 
	id = gImageHandler->AddImage2DTiled(gAllocationSubmissions[int(myThreadID)], path, tiling.y, tiling.x);
	return ImageHandle(id);
}

rflx::ImageHandle
rflx::Reflex::CreateImage(
	std::vector<PixelValue>&& data,
	Vec2f						tiling) const
{
	tiling = tiling; // TODO: IMPLEMENT
	std::vector<uint8_t> dataAligned(data.size() * 4);
	memcpy(dataAligned.data(), data.data(), data.size() * sizeof PixelValue);
	float dim = sqrtf(float(data.size()));
	ImageID id = gImageHandler->AddImage2D(gAllocationSubmissions[int(myThreadID)], std::move(dataAligned), { dim, dim });
	return ImageHandle(id);
}

rflx::CubeHandle
rflx::Reflex::CreateImageCube(
	const char* path) const
{
	CubeHandle handle(gImageHandler->AddImageCube(gAllocationSubmissions[int(myThreadID)], path));

	const float fDim = handle.GetDim();
	const CubeDimension cubeDim = fDim == 2048 ? CubeDimension::Dim2048 : fDim == 1024 ? CubeDimension::Dim1024 : CubeDimension::Dim1;
	gCubeFilterer->PushFilterWork(handle.GetID(), cubeDim);

	return handle;
}

void
rflx::Reflex::SetUseRayTracing(
	bool useFlag)
{
	*gUseRayTracing = useFlag;
}

void
rflx::Reflex::BeginPush()
{
	if (*gUseRayTracing)
	{
		gRTMeshRenderer->myWorkScheduler.BeginPush(myThreadID);
	}
	else
	{
		gMeshRenderer->myWorkScheduler.BeginPush(myThreadID);
	}

	gSpriteRenderer->myWorkScheduler.BeginPush(myThreadID);

	gAllocationSubmissions[int(myThreadID)] = ourVKImplementation->myAllocationSubmitter->StartAllocSubmission(myThreadID);
}

void
rflx::Reflex::PushRenderCommand(
	const MeshHandle& handle,
	const Vec3f& position,
	const Vec3f& scale,
	const Vec3f& forward,
	float				rotation)
{
	MeshRenderCommand cmd{};
	cmd.id = handle.GetID();
	
	cmd.transform =
		glm::translate(glm::identity<Mat4f>(), position) *
		glm::rotate(glm::identity<Mat4f>(), rotation, forward) *
		glm::scale(glm::identity<Mat4f>(), scale);

	if (*gUseRayTracing)
	{
		gRTMeshRenderer->myWorkScheduler.PushWork(myThreadID, cmd);
	}
	else
	{
		gMeshRenderer->myWorkScheduler.PushWork(myThreadID, cmd);
	}

}

void
rflx::Reflex::PushRenderCommand(
	FontID			fontID,
	const char* text,
	const Vec3f& position,
	float			scale,
	const Vec4f& color)
{


	auto [tw, th] = gVulkanFramework->GetTargetResolution();
	float ratio = th / tw;
	float colOffset = 0;
	float rowOffset = 0;
	for (int charIndex = 0; charIndex < 128; ++charIndex)
	{
		if (text[charIndex] == '\0')
		{
			break;
		}

		if (text[charIndex] == '\n')
		{
			colOffset = 0;
			rowOffset += scale;
			continue;
		}

		Font font = (*gFontHandler)[fontID];
		SpriteRenderCommand cmd{};
		cmd.imgArrID = font.imgArrID;
		cmd.imgArrIndex = text[charIndex] - FirstFontGlyph;

		GlyphMetrics metrics = font.metrics[cmd.imgArrIndex];
		metrics.xStride *= scale;
		metrics.yOffset *= scale;
		cmd.position = { position.x + colOffset * ratio, position.y + metrics.yOffset + rowOffset };

		cmd.pivot = { 0, -1 };
		cmd.color = color;
		cmd.scale = { scale, scale };

		colOffset += metrics.xStride;

		gSpriteRenderer->myWorkScheduler.PushWork(myThreadID, cmd);
	}
}

void
rflx::Reflex::PushRenderCommand(
	ImageHandle handle,
	uint32_t    subImg,
	Vec2f		position,
	Vec2f		scale,
	Vec2f		pivot,
	Vec4f		color)
{
	SpriteRenderCommand cmd{};
	cmd.imgArrID = handle.GetID();
	cmd.imgArrIndex = subImg;
	cmd.position = { position.x * my2DScaleRef.x, position.y * my2DScaleRef.y };
	auto sScale = (*gImageHandler)[handle.GetID()].scale;
	cmd.scale = scale * sScale;
	cmd.pivot = pivot;
	cmd.color = color;

	gSpriteRenderer->myWorkScheduler.PushWork(myThreadID, cmd);
}

void
rflx::Reflex::EndPush()
{
	if (*gUseRayTracing)
	{
		gRTMeshRenderer->myWorkScheduler.EndPush(myThreadID);
	}
	else
	{
		gMeshRenderer->myWorkScheduler.EndPush(myThreadID);
	}

	gSpriteRenderer->myWorkScheduler.EndPush(myThreadID);
	ourVKImplementation->myAllocationSubmitter->QueueAllocSubmission(std::move(gAllocationSubmissions[int(myThreadID)]));
}

void
rflx::Reflex::SetView(
	const Vec3f& position,
	const Vec2f& rotation,
	float			distance)
{
	gSceneGlobals->SetView(position, rotation, distance);
}

void
rflx::Reflex::SetScaleReference(
	ImageHandle	handle,
	Vec2f		coefficient)
{
	const Vec2f dim = handle.GetDimensions();
	my2DScaleRef.x = 1.f / dim.x;
	my2DScaleRef.y = 1.f / dim.y;

	my2DScaleRef *= coefficient;
}
