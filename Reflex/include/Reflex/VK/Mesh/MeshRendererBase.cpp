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
MeshRendererBase::AddSchedule(
	neat::ThreadID threadID)
{
	myWorkScheduler.AddSchedule(threadID);
}

std::array<VkFence, NumSwapchainImages>
MeshRendererBase::GetFences()
{
    return myCmdBufferFences;
}
