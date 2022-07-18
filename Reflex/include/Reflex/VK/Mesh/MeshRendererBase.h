
#pragma once
#include "MeshRenderCommand.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"
#include "neat/Misc/TripleBuffer.h"

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

														void AddSchedule(neat::ThreadID threadID) override;

	WorkScheduler<MeshRenderCommand, 1024, 1024>		myWorkScheduler;

protected:
	VulkanFramework&									theirVulkanFramework;
	UniformHandler&										theirUniformHandler;
	MeshHandler&										theirMeshHandler;
	ImageHandler&										theirImageHandler;
	SceneGlobals&										theirSceneGlobals;


	std::array<VkCommandBuffer, NumSwapchainImages>		myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>				myCmdBufferFences;


};
