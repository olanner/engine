
#pragma once
#include "WorkScheduler.h"
#include "Reflex/Features.h"

struct WaitSemaphores
{
	std::array<VkSemaphore, 16>				semaphores;
	std::array<VkPipelineStageFlags, 16>	stages;
};

struct SlottedWorkerSystem
{
	VkQueue subQueue;
	VkPipelineStageFlags							waitStage;
	std::array<VkSemaphore, NumSwapchainImages>		signalSemaphores;
	std::shared_ptr<class WorkerSystem>				system;
};

void SubmitWorkerSystemCmds(
		SlottedWorkerSystem&	workerSystem, 
		VkDevice				device, 
		uint32_t				swapchainIndex);
class WorkerSystem
{
public:
	[[nodiscard]] virtual std::tuple<VkSubmitInfo, VkFence> RecordSubmit(
																uint32_t				swapchainImageIndex,
																VkSemaphore*			waitSemaphores,
																uint32_t				numWaitSemaphores,
																VkPipelineStageFlags*	waitPipelineStages,
																uint32_t				numWaitStages,
																VkSemaphore*			signalSemaphore) = 0;
	virtual void											AddSchedule(neat::ThreadID threadID) {}
	virtual std::array<VkFence, NumSwapchainImages>			GetFences() = 0;
	virtual std::vector<rflx::Features> GetImplementedFeatures() const = 0;
private:
	
	
};