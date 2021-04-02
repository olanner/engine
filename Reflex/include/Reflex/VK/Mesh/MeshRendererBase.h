
#pragma once
#include "MeshRenderCommand.h"

class MeshRendererBase
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

	void											BeginPush();
	void											PushRenderCommand( 
														const MeshRenderCommand& command);
	void											EndPush();

protected:
	VulkanFramework&								theirVulkanFramework;
	UniformHandler&									theirUniformHandler;
	MeshHandler&									theirMeshHandler;
	ImageHandler&									theirImageHandler;
	SceneGlobals&									theirSceneGlobals;

	std::mutex										mySwapMutex;
	uint8_t											myPushIndex = 0;
	uint8_t											myFreeIndex = 1;
	uint8_t											myRecordIndex = 2;
	std::array<neat::static_vector<MeshRenderCommand, MaxNumInstances>, 3>
													myRenderCommands{};

	std::array<VkCommandBuffer, NumSwapchainImages>	myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;


};
