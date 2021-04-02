
#pragma once
#include "Reflex/VK/Mesh/MeshRendererBase.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"

class RTMeshRenderer : public MeshRendererBase, public WorkerSystem
{
public:
									RTMeshRenderer(
										class VulkanFramework&				vulkanFramework,
										class UniformHandler&				uniformHandler,
										class MeshHandler&					meshHandler,
										class ImageHandler&					imageHandler,
										class SceneGlobals&					sceneGlobals,
										class BufferAllocator&				bufferAllocator,
										class AccelerationStructureHandler& accStructHandler,
										QueueFamilyIndex					cmdBufferFamily,
										QueueFamilyIndex					transferFamily);
									~RTMeshRenderer();

	[[nodiscard]] std::tuple<VkSubmitInfo, VkFence>
									RecordSubmit(
										uint32_t				swapchainImageIndex,
										VkSemaphore*			waitSemaphores,
										uint32_t				numWaitSemaphores,
										VkPipelineStageFlags*	waitPipelineStages,
										uint32_t				numWaitStages,
										VkSemaphore*			signalSemaphore) override;

private:
	BufferAllocator&				theirBufferAllocator;
	AccelerationStructureHandler&	theirAccStructHandler;

	neat::static_vector<QueueFamilyIndex, 8>
									myOwners;

	VkPipelineLayout				myRTPipelineLayout;
	VkPipeline						myRTPipeLine;
	class Shader*					myOpaqueShader;

	VkBuffer						myShaderBindingTable;
	VkDeviceMemory					mySBTMemory;
	uint32_t						myShaderProgramSize;
	uint32_t						myNumMissShaders;
	uint32_t						myNumClosestHitShaders;

	InstanceStructID				myInstancesID;

};