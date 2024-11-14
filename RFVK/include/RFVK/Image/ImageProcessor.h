#pragma once
#include "RFVK/RenderPass/RenderPass.h"
#include "RFVK/WorkerSystem/WorkerSystem.h"
#include "RFVK/Pipelines/Pipeline.h"

struct ImageProcessingWork
{
	Pipeline						pipeline;
	Vec2f							imageResolution;
	VkImage							imageToProcess;
	VkImageLayout					targetLayout;
};

class ImageProcessor : public WorkerSystem
{
public:
	ImageProcessor(
		class VulkanFramework& 		vulkanFramework,
		class ImageAllocator& 		imageAllocator,
		class ImageHandler& 		imageHandler,
		QueueFamilyIndices			familyIndices);
	neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
											RecordSubmit(
												uint32_t swapchainImageIndex, 
												const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores, 
												const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores) override;
	std::array<VkFence, NumSwapchainImages>	GetFences() override;
	std::vector<rflx::Features>				GetImplementedFeatures() const override;
	int										GetSubmissionCount() override { return 1; }

private:
	VkResult						CreateDescriptorSet();
	VulkanFramework& 				theirVulkanFramework;
	ImageAllocator& 				theirImageAllocator;
	ImageHandler& 					theirImageHandler;
	QueueFamilyIndex				myComputeQueueIndex = 0;
	concurrency::concurrent_queue<ImageProcessingWork>
									myToDoWork;

	std::array<VkPipelineStageFlags, MaxWorkerSubmissions>
									myWaitStages;
	std::shared_ptr<class Shader>	myImageProcessShader;
	Pipeline						myImageProcessPipeline = {};

	VkDescriptorPool				myDescriptorPool = nullptr;
	VkDescriptorSetLayout			myDescriptorLayout = nullptr;
	VkDescriptorSet					myDescriptorSet = nullptr;

	std::array<VkCommandBuffer, NumSwapchainImages>	myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;

	bool myHasRun = false; // TODO: REMOVE
};