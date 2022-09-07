
#pragma once
#include "RFVK/Pipelines/Pipeline.h"
#include "RFVK/RenderPass/RenderPassFactory.h"
#include "Shared.h"
#include "RFVK/Mesh/MeshRenderCommand.h"
#include "RFVK/Ray Tracing/AccelerationStructureHandler.h"
#include "RFVK/Ray Tracing/AccelerationStructureHandler.h"
#include "RFVK/WorkerSystem/WorkScheduler.h"

class DeferredGeoRenderer final
{
	struct Instance
	{
		Mat4f		mat;
		Vec3f		padding0;
		uint32_t	objID;
	};
	static_assert(128 > sizeof Instance);
	struct UniformInstances
	{
		Instance instances[MaxNumInstances];
	};

public:
			DeferredGeoRenderer(
				class VulkanFramework&		vulkanFramework,
				class UniformHandler&		uniformHandler,
				class MeshHandler&			meshHandler,
				class ImageHandler&			imageHandler,
				class SceneGlobals&			sceneGlobals,
				class RenderPassFactory&	renderPassFactory,
				GBuffer						gBuffer,
				QueueFamilyIndices			familyIndices);
			~DeferredGeoRenderer();

	void	Record(
				int												swapchainIndex,
				VkCommandBuffer									cmdBuffer,
				const MeshRenderSchedule::AssembledWorkType&	assembledWork);

	

private:
	VulkanFramework&	theirVulkanFramework;
	UniformHandler&		theirUniformHandler;
	MeshHandler&		theirMeshHandler;
	ImageHandler&		theirImageHandler;
	SceneGlobals&		theirSceneGlobals;
	RenderPassFactory&	theirRenderPassFactory;

	std::unique_ptr<UniformInstances>	myInstanceData;
	UniformID							myInstanceUniformID = UniformID(INVALID_ID);

	RenderPass							myDeferredRenderPass;

	std::shared_ptr<class Shader>		myDeferredGeoShader;
	Pipeline							myDeferredGeoPipeline;

};
