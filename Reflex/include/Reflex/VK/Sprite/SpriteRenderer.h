
#pragma once
#include "Reflex/VK/Pipelines/Pipeline.h"
#include "Reflex/VK/RenderPass/RenderPass.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"


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
																class VulkanFramework&	vulkanFramework,
																class SceneGlobals&		sceneGlobals,
																class ImageHandler&		imageHandler,
																class RenderPassFactory&	renderPassFactory,
																class UniformHandler&		uniformHandler,
																QueueFamilyIndex* 			firstOwner, 
																uint32_t					numOwners,
																uint32_t					cmdBufferFamily);

	std::tuple<VkSubmitInfo, VkFence>						RecordSubmit(
																uint32_t					swapchainImageIndex,
																VkSemaphore*				waitSemaphores,
																uint32_t					numWaitSemaphores,
																VkPipelineStageFlags*		waitPipelineStages,
																uint32_t					numWaitStages,
																VkSemaphore*				signalSemaphore) override;
	void													AddSchedule(ScheduleID scheduleID) override { myWorkScheduler.AddSchedule(scheduleID); }
	WorkScheduler<SpriteRenderCommand, 1024, 1024>			myWorkScheduler;

private:
	VulkanFramework&										theirVulkanFramework;
	SceneGlobals&											theirSceneGlobals;
	ImageHandler&											theirImageHandler;
	UniformHandler&											theirUniformHandler;

	neat::static_vector<QueueFamilyIndex, 8>				myOwners;

	RenderPass												myRenderPass;

	class Shader*											mySpriteShader;
	Pipeline												mySpritePipeline;

	std::array<VkCommandBuffer, NumSwapchainImages>			myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>					myCmdBufferFences;

	UniformID												mySpriteInstancesID;

	
};
