#include "pch.h"
#include "WorkerSystem.h"

void
SubmitWorkerSystemCmds(
	SlottedWorkerSystem&	workerSystem, 
	VkDevice				device, 
	uint32_t				swapchainIndex)
{
	auto [submitInfo, fence] = workerSystem.system->RecordSubmit(swapchainIndex,
																  workerSystem.waitSemaphores[swapchainIndex].semaphores.data(),
																  workerSystem.numWaitSemaphores,
																  workerSystem.waitSemaphores[swapchainIndex].stages.data(),
																  workerSystem.numWaitSemaphores,
																  &workerSystem.signalSemaphores[swapchainIndex]
	);
	vkResetFences(device, 1, &fence);
	auto resultSubmit = vkQueueSubmit(workerSystem.subQueue, 1, &submitInfo, fence);
	assert(!resultSubmit && "failed submission");
}
