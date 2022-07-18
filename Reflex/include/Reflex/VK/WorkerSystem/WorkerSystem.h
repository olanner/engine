
#pragma once
#include "WorkScheduler.h"

struct WaitSemaphores
{
	std::array<VkSemaphore, 16>				semaphores;
	std::array<VkPipelineStageFlags, 16>	stages;
};

struct SlottedWorkerSystem
{
	VkQueue subQueue;

	std::array<WaitSemaphores, NumSwapchainImages>	waitSemaphores;
	uint32_t										numWaitSemaphores;
	void											AddWaitSemaphore(
														const std::array<VkSemaphore, NumSwapchainImages>&	semaphores, 
														VkPipelineStageFlags								stage)
	{
		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			waitSemaphores[swapchainIndex].semaphores[numWaitSemaphores] = semaphores[swapchainIndex];
			waitSemaphores[swapchainIndex].stages[numWaitSemaphores] = stage;
		}
		numWaitSemaphores++;
	}

	std::array<VkSemaphore, NumSwapchainImages>		signalSemaphores;
	std::shared_ptr<class WorkerSystem>				system;
	bool											isActive;
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
private:
	
	
};