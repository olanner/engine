
#pragma once
#include "RFVK/Pipelines/Pipeline.h"
#include "RFVK/RenderPass/RenderPass.h"
#include "RFVK/WorkerSystem/WorkerSystem.h"


struct SpriteRenderCommand
{
	Vec4f			color;
	Vec2f			position;
	Vec2f			pivot;
	Vec2f			scale;
	ImageID			imgArrID;
	uint32_t		imgArrIndex;
};

struct SpriteInstance
{
	Vec4f padding0;
	Vec4f color;
	Vec2f pos;
	Vec2f pivot;
	Vec2f scale;
	float imgArrID;
	float imgArrIndex;
};

struct SpriteRenderSchedule
{
	std::array<neat::static_vector<SpriteRenderCommand, MaxNumInstances>, 3> renderCommands;
	uint8_t pushIndex = 0;
	uint8_t freeIndex = 1;
	uint8_t recordIndex = 2;
};

class SpriteRenderer : public WorkerSystem
{
public:
															SpriteRenderer(
																class VulkanFramework&		vulkanFramework,
																class SceneGlobals&			sceneGlobals,
																class ImageHandler&			imageHandler,
																class RenderPassFactory&	renderPassFactory,
																class UniformHandler&		uniformHandler,
																QueueFamilyIndices			familyIndices);

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
															RecordSubmit(
																uint32_t	swapchainImageIndex, 
																const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& 
																			waitSemaphores, 
																const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& 
																			signalSemaphores) override;
	void													AddSchedule(neat::ThreadID threadID) override { myWorkScheduler.AddSchedule(threadID); }
	std::array<VkFence, NumSwapchainImages>					GetFences() override;
	std::vector<rflx::Features>								GetImplementedFeatures() const override;
	int														GetSubmissionCount() override { return 1; }
	
	WorkScheduler<SpriteRenderCommand, 1024, 1024>			myWorkScheduler;

private:
	VulkanFramework&										theirVulkanFramework;
	SceneGlobals&											theirSceneGlobals;
	ImageHandler&											theirImageHandler;
	UniformHandler&											theirUniformHandler;

	neat::static_vector<QueueFamilyIndex, 8>				myOwners;

	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
															myWaitStages;
	RenderPass												myRenderPass;
	class Shader*											mySpriteShader;
	Pipeline												mySpritePipeline;

	std::array<VkCommandBuffer, NumSwapchainImages>			myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>					myCmdBufferFences;

	UniformID												mySpriteInstancesID;

	
};
