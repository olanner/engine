#pragma once
#include "Shared.h"
#include "RFVK/WorkerSystem/WorkerSystem.h"

class DeferredRayTracer : public WorkerSystem
{
public:
	DeferredRayTracer(
		class VulkanFramework&		vulkanFramework,
		class UniformHandler&		uniformHandler,
		class MeshHandler&			meshHandler,
		class ImageHandler&			imageHandler,
		class SceneGlobals&			sceneGlobals,
		class RenderPassFactory&	renderPassFactory,
		class BufferAllocator&		bufferAllocator,
		class AccelerationStructureHandler&		
									accStructHandler,
		QueueFamilyIndices			familyIndices);
	~DeferredRayTracer();

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
												RecordSubmit(
													uint32_t swapchainImageIndex, 
													const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores, 
													const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores) override;
	std::vector<rflx::Features>					GetImplementedFeatures() const override;
	std::array<VkFence, NumSwapchainImages>		GetFences() override;
	int											GetSubmissionCount() override { return 2; }

	void										AddSchedule(neat::ThreadID threadID) override { myWorkScheduler.AddSchedule(threadID); }
	MeshRenderSchedule							myWorkScheduler;

private:
	VulkanFramework&								theirVulkanFramework;

	VkDescriptorPool								myDescriptorPool = nullptr;
	GBuffer											myGBuffer = {};
	
	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
													myGeoWaitStages;
	std::shared_ptr<class DeferredGeoRenderer>		myGeoRenderer;
	std::array<VkCommandBuffer, NumSwapchainImages>	myGeoCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myGeoCmdBufferFences;

	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
													myRTWaitStages;
	std::shared_ptr<class RayTracer>				myRayTracer;
	std::array<VkCommandBuffer, NumSwapchainImages>	myRTCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myRTCmdBufferFences;

	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
													mySubmissions;
};