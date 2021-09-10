#include "pch.h"
#include "MeshRendererBase.h"

#include "Reflex/VK/VulkanFramework.h"

MeshRendererBase::MeshRendererBase(
	VulkanFramework&	vulkanFramework,
	UniformHandler&		uniformHandler,
	MeshHandler&		meshHandler,
	ImageHandler&		imageHandler,
	SceneGlobals&		sceneGlobals,
	QueueFamilyIndex	cmdBufferFamily)
	: theirVulkanFramework(vulkanFramework)
	, theirUniformHandler(uniformHandler)
	, theirMeshHandler(meshHandler)
	, theirImageHandler(imageHandler)
	, theirSceneGlobals(sceneGlobals)
{
	// COMMANDS
	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		auto [resultBuffer, buffer] = theirVulkanFramework.RequestCommandBuffer(cmdBufferFamily);
		assert(!resultBuffer && "failed creating cmd buffer");
		myCmdBuffers[i] = buffer;
	}

	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		auto resultFence = vkCreateFence(theirVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myCmdBufferFences[i]);
		assert(!resultFence && "failed creating fences");
	}

}

MeshRendererBase::~MeshRendererBase()
{
	for (uint32_t i = 0; i < NumSwapchainImages; i++)
	{
		vkDestroyFence(theirVulkanFramework.GetDevice(), myCmdBufferFences[i], nullptr);
	}
}

void
MeshRendererBase::BeginPush(
	ScheduleID scheduleID)
{
	auto& schedule = mySchedules[scheduleID];
	{
		std::scoped_lock lock(mySwapMutex);
		std::swap(schedule.pushIndex, schedule.freeIndex);
	}
	schedule.renderCommands[schedule.pushIndex].clear();
}

void
MeshRendererBase::PushRenderCommand(
	ScheduleID scheduleID,
	const MeshRenderCommand& command)
{
	auto& schedule = mySchedules[scheduleID];
	schedule.renderCommands[schedule.pushIndex].emplace_back(command);
}

void
MeshRendererBase::EndPush(
	ScheduleID scheduleID)
{
}

void MeshRendererBase::AssembleScheduledWork()
{
	std::scoped_lock lock(mySwapMutex);
	
	myAssembledRenderCommands.clear();
	for (auto& [id, schedule] : mySchedules)
	{
		std::swap(schedule.recordIndex, schedule.freeIndex);

		for (auto& cmd : schedule.renderCommands[schedule.recordIndex])
		{
			myAssembledRenderCommands.emplace_back(cmd);
		}
	}
	if (myAssembledRenderCommands.empty())
	{
		int val = 0;
	}
}

void
MeshRendererBase::AddSchedule(
	ScheduleID scheduleID)
{
	std::scoped_lock lock(mySwapMutex);
	mySchedules[scheduleID] = {};
}