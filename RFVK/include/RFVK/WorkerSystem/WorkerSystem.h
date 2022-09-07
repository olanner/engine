
#pragma once
#include "WorkScheduler.h"
#include "RFVK/Features.h"

struct WaitSemaphores
{
	std::array<VkSemaphore, 16>				semaphores;
	std::array<VkPipelineStageFlags, 16>	stages;
};

constexpr int MaxWorkerSubmissions = 8;
struct SlottedWorkerSystem
{
	std::array<neat::static_vector<VkSemaphore, MaxWorkerSubmissions>, NumSwapchainImages>
													signalSemaphores;
	std::shared_ptr<class WorkerSystem>				system;
};

struct WorkerSubmission
{
	VkFence			fence;
	VkSubmitInfo	submitInfo;
	VkQueueFlagBits desiredQueue;
};
class WorkerSystem
{
public:
	[[nodiscard]] virtual neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
															RecordSubmit(
																uint32_t				swapchainImageIndex,
																const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&
																						waitSemaphores,
																const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&
																						signalSemaphores) = 0;
	virtual void											AddSchedule(neat::ThreadID threadID) {}
	virtual std::array<VkFence, NumSwapchainImages>			GetFences() = 0;
	virtual int												GetSubmissionCount() = 0;
	virtual std::vector<rflx::Features>						GetImplementedFeatures() const = 0;
private:
	
	
};