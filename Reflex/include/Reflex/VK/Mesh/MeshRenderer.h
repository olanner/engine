
#pragma once
#include "MeshRendererBase.h"
#include "Reflex/VK/Pipelines/Pipeline.h"
#include "Reflex/VK/RenderPass/RenderPassFactory.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"

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

class MeshRenderer final : public MeshRendererBase
{
public:
										MeshRenderer(
											class VulkanFramework& vulkanFramework,
											class UniformHandler& uniformHandler,
											class MeshHandler& meshHandler,
											class ImageHandler& imageHandler,
											class SceneGlobals& sceneGlobals,
											class RenderPassFactory& renderPassFactory,
											QueueFamilyIndex cmdBufferFamily);
										~MeshRenderer();

	[[nodiscard]] std::tuple<VkSubmitInfo, VkFence>
										RecordSubmit(
											uint32_t				swapchainImageIndex,
											VkSemaphore*			waitSemaphores,
											uint32_t				numWaitSemaphores,
											VkPipelineStageFlags*	waitPipelineStages,
											uint32_t				numWaitStages,
											VkSemaphore*			signalSemaphore) override;
	std::vector<rflx::Features>			GetImplementedFeatures() const override;

private:
	RenderPassFactory&					theirRenderPassFactory;

	std::unique_ptr<UniformInstances>	myInstanceData;
	UniformID							myInstanceUniformID = UniformID(INVALID_ID);

	RenderPass							myDeferredRenderPass;

	class Shader*						myDeferredGeoShader;
	Pipeline							myDeferredGeoPipeline;

	class Shader*						myDeferredLightShader;
	Pipeline							myDeferredLightPipeline;

};