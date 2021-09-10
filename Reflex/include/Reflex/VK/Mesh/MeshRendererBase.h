
#pragma once
#include "MeshRenderCommand.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"


struct MeshRenderSchedule
{
	std::array<std::vector<MeshRenderCommand>, 3> renderCommands;
	uint8_t pushIndex = 0;
	uint8_t freeIndex = 1;
	uint8_t recordIndex = 2;
};

class MeshRendererBase : public WorkerSystem
{
public:
														MeshRendererBase( 
															class VulkanFramework&	vulkanFramework,
															class UniformHandler&	uniformHandler,
															class MeshHandler&		meshHandler,
															class ImageHandler&		imageHandler,
															class SceneGlobals&		sceneGlobals,
															QueueFamilyIndex		cmdBufferFamily);
														~MeshRendererBase();

	void												BeginPush(
															ScheduleID scheduleID);
	void												PushRenderCommand(
															ScheduleID scheduleID,
															const MeshRenderCommand& command);
	void												EndPush(
															ScheduleID scheduleID);
	void												AssembleScheduledWork();
	void												AddSchedule(
															ScheduleID scheduleID) override;

protected:
	VulkanFramework&									theirVulkanFramework;
	UniformHandler&										theirUniformHandler;
	MeshHandler&										theirMeshHandler;
	ImageHandler&										theirImageHandler;
	SceneGlobals&										theirSceneGlobals;

	std::mutex											mySwapMutex;
	std::mutex											myScheduleMutex;
	std::unordered_map<ScheduleID, MeshRenderSchedule>	mySchedules;
	neat::static_vector<MeshRenderCommand, MaxNumInstances>
														myAssembledRenderCommands;

	std::array<VkCommandBuffer, NumSwapchainImages>		myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>				myCmdBufferFences;


};
