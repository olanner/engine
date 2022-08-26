
#pragma once
#include "NVRayTracing.h"
#include "RFVK/Mesh/MeshRendererBase.h"

struct ShaderBindingTable
{
	VkBuffer						buffer;
	VkDeviceMemory					memory;
	VkStridedDeviceAddressRegionKHR stride;
};

class RTMeshRenderer final : public MeshRendererBase
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
	std::vector<rflx::Features>			GetImplementedFeatures() const override;

private:
	ShaderBindingTable					CreateShaderBindingTable(
											class AllocationSubmission& allocSub,
											size_t						handleSize,
											uint32_t					firstGroup,
											uint32_t					groupCount,
											VkPipeline					pipeline
											);
	uint32_t							AlignedSize(
											uint32_t a,
											uint32_t b
												);
	
	BufferAllocator&					theirBufferAllocator;
	AccelerationStructureHandler&		theirAccStructHandler;

	std::vector<QueueFamilyIndex>		myOwners;

	VkPipelineLayout					myRTPipelineLayout;
	VkPipeline							myRTPipeLine;
	class Shader*						myOpaqueShader;

	std::array<ShaderBindingTable, 3>	myShaderBindingTables;
	//VkBuffer						mySBTBuffer;
	//VkDeviceMemory					mySBTMemory;
	//VkDeviceAddress					mySBTAddress;
	//VkStridedDeviceAddressRegionKHR myMissStride;
	//VkStridedDeviceAddressRegionKHR myRGenStride;
	//VkStridedDeviceAddressRegionKHR myCHitStride;

	InstanceStructID					myInstancesID;
	RTInstances							myInstances;

};
