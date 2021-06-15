#include "pch.h"
#include "Reflex.h"
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

bool*							gUseRayTracing;

//CORE
std::shared_ptr<SceneGlobals>					gSceneGlobals;
std::shared_ptr<MeshHandler>					gMeshHandler;
std::shared_ptr<ImageHandler>					gImageHandler;
std::shared_ptr<FontHandler>					gFontHandler;
std::shared_ptr<CubeFilterer>					gCubeFilterer;
std::shared_ptr<AccelerationStructureHandler>	gAccStructHandler;

// FEATURES
std::shared_ptr<MeshRenderer>					gMeshRenderer;
std::shared_ptr<RTMeshRenderer>					gRTMeshRenderer;
std::shared_ptr<SpriteRenderer>					gTextRenderer;

VulkanFramework*								gVulkanFramework;

Reflex::Reflex(
	const WindowInfo&	windowInformation,
	const char*			cmdArgs)
	: myVKImplementation(nullptr)
	, myIsGood(true)
{
	bool useDebugLayers = false;
	if (cmdArgs)
	{
		useDebugLayers = std::string(cmdArgs).find("vkdebug") != std::string::npos;
	}

	myVKImplementation = new VulkanImplementation;
	myIsGood = !myVKImplementation->Init(windowInformation, useDebugLayers);



	auto mr = std::make_shared<MeshRenderer>(myVKImplementation->myVulkanFramework,
											*myVKImplementation->myUniformHandler,
										 *myVKImplementation->myMeshHandler,
										 *myVKImplementation->myImageHandler,
										 *myVKImplementation->mySceneGlobals,
										 *myVKImplementation->myRenderPassFactory,
										 myVKImplementation->myPresQueueIndex);

	auto rtmr = std::make_shared<RTMeshRenderer>(myVKImplementation->myVulkanFramework,
											   *myVKImplementation->myUniformHandler,
											   *myVKImplementation->myMeshHandler,
											   *myVKImplementation->myImageHandler,
											   *myVKImplementation->mySceneGlobals,
											   *myVKImplementation->myBufferAllocator,
											   *myVKImplementation->myAccStructHandler,
											   myVKImplementation->myCompQueueIndex,
											   myVKImplementation->myTransQueueIndex
	);

	QueueFamilyIndex indices[]{myVKImplementation->myPresQueueIndex, myVKImplementation->myTransQueueIndex};
	auto tr = std::make_shared<SpriteRenderer>(myVKImplementation->myVulkanFramework,
										*myVKImplementation->mySceneGlobals,
										*myVKImplementation->myImageHandler,
										*myVKImplementation->myRenderPassFactory,
										*myVKImplementation->myUniformHandler,
										indices,
										ARRAYSIZE(indices),
										myVKImplementation->myPresQueueIndex);

	myVKImplementation->RegisterWorkerSystem(rtmr,
	                                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV,
	                                         VK_QUEUE_COMPUTE_BIT);

	myVKImplementation->RegisterWorkerSystem(mr, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);
	myVKImplementation->RegisterWorkerSystem(tr, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_QUEUE_GRAPHICS_BIT);

	myVKImplementation->LockWorkerSystems();

	gMeshRenderer = mr;
	gSceneGlobals = myVKImplementation->mySceneGlobals;
	gMeshHandler = myVKImplementation->myMeshHandler;
	gAccStructHandler = myVKImplementation->myAccStructHandler;
	gRTMeshRenderer = rtmr;
	gImageHandler = myVKImplementation->myImageHandler;
	gCubeFilterer = myVKImplementation->myCubeFilterer;
	gTextRenderer = tr;
	gFontHandler = myVKImplementation->myFontHandler;

	gUseRayTracing = &myVKImplementation->myUseRayTracing;

	gVulkanFramework = &myVKImplementation->myVulkanFramework;
}

Reflex::~Reflex()
{
	SAFE_DELETE(myVKImplementation);
}

bool
Reflex::IsGood()
{
	return myIsGood;
}

void
Reflex::BeginFrame()
{
	myVKImplementation->BeginFrame();
}

void
Reflex::Submit()
{
	myVKImplementation->Submit();
}

void
Reflex::EndFrame()
{
	myVKImplementation->EndFrame();
}

MeshHandle
rflx::LoadMesh(
	const char*					 path,
	std::vector<ImageHandle>&&	 imgHandles)
{
	std::vector<ImageID> imgIDs;
	for (auto& handle : imgHandles)
	{
		imgIDs.emplace_back(handle.GetID());
	}
	MeshID id = gMeshHandler->AddMesh(path, std::move(imgIDs));
	MeshHandle ret(id);

	//if ( *gUseRayTracing )
	//{
	Mesh mesh = (*gMeshHandler)[id];
	assert(!gAccStructHandler->AddGeometryStructure(id, &mesh, 1) && "failed creating geo structure");
	//}

	return ret;
}

ImageHandle
rflx::LoadImage2D(
	const char* path)
{
	ImageID id = gImageHandler->AddImage2D(path);
	return ImageHandle(id);
}

ImageHandle
rflx::LoadImage2D(
	std::vector<PixelValue>&& data)
{
	std::vector<char> dataAligned(data.size() * 4);
	memcpy(dataAligned.data(), data.data(), data.size() * sizeof PixelValue);
	data.resize(0);
	ImageID id = gImageHandler->AddImage2D(std::move(dataAligned));
	return ImageHandle(id);
}

CubeHandle
rflx::LoadImageCube(
	const char* path)
{
	CubeHandle handle(gImageHandler->AddImageCube(path));
	handle.theirSceneGlobals = gSceneGlobals;

	const float fDim = gImageHandler->GetImageCubeDimension(handle.GetID());
	const CubeDimension cubeDim = fDim == 2048 ? CubeDimension::Dim2048 : fDim == 1024 ? CubeDimension::Dim1024 : CubeDimension::Dim1;
	gCubeFilterer->PushFilterWork(handle.GetID(), cubeDim);

	return handle;
}

void
rflx::SetUseRayTracing(
	bool useFlag)
{
	*gUseRayTracing = useFlag;
}

void
rflx::BeginPush()
{
	if (*gUseRayTracing)
	{
		gRTMeshRenderer->BeginPush();
	}
	else
	{
		gMeshRenderer->BeginPush();
	}

	gTextRenderer->BeginPush();
}

void
rflx::PushRenderCommand(
	const MeshHandle& handle,
	const Vec3f& position,
	const Vec3f& scale,
	const Vec3f& forward,
	float				rotation)
{
	MeshRenderCommand cmnd{};
	cmnd.id = handle.GetID();

	cmnd.transform =
		glm::translate(glm::identity<Mat4f>(), position) *
		glm::rotate(glm::identity<Mat4f>(), rotation, forward) *
		glm::scale(glm::identity<Mat4f>(), scale);

	if (*gUseRayTracing)
	{
		gRTMeshRenderer->PushRenderCommand(cmnd);
	}
	else
	{
		gMeshRenderer->PushRenderCommand(cmnd);
	}

}

void
rflx::PushRenderCommand(
	FontID			fontID,
	const char*		text,
	const Vec3f&	position,
	float			scale,
	const Vec4f&	color)
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
		cmd.scale = scale;

		colOffset += metrics.xStride;

		gTextRenderer->PushRenderCommand(cmd);
	}
}

void
rflx::EndPush()
{
	if (*gUseRayTracing)
	{
		gRTMeshRenderer->EndPush();
	}
	else
	{
		gMeshRenderer->EndPush();
	}

	gTextRenderer->EndPush();
}

void
rflx::SetView(
	const Vec3f& position,
	const Vec2f& rotation,
	float			distance)
{
	gSceneGlobals->SetView(position, rotation, distance);
}
