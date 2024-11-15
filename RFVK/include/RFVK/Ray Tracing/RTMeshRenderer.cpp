#include "pch.h"
#include "RTMeshRenderer.h"

#include "AccelerationStructureHandler.h"
#include "NVRayTracing.h"
#include "RFVK/Image/ImageHandler.h"
#include "RFVK/Memory/BufferAllocator.h"
#include "RFVK/Mesh/MeshHandler.h"
#include "RFVK/Scene/SceneGlobals.h"
#include "RFVK/Shader/Shader.h"
#include "RFVK/VulkanFramework.h"
#include "RTPipelineBuilder.h"

RTMeshRenderer::RTMeshRenderer(
	VulkanFramework&				vulkanFramework,
	UniformHandler&					uniformHandler,
	MeshHandler&					meshHandler,
	ImageHandler&					imageHandler,
	SceneGlobals&					sceneGlobals,
	BufferAllocator&				bufferAllocator,
	AccelerationStructureHandler&	accStructHandler,
	QueueFamilyIndices				familyIndices)
	: MeshRendererBase(vulkanFramework,
					  uniformHandler,
					  meshHandler,
					  imageHandler,
					  sceneGlobals,
						familyIndices[QUEUE_FAMILY_COMPUTE])
	, theirBufferAllocator(bufferAllocator)
	, theirAccStructHandler(accStructHandler)
{
	myWaitStages.fill(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);
	// SHADERS
	char shaderPaths[][128]
	{
		"Shaders/ray_rgen.rgen",
		"Shaders/ray_rmiss.rmiss",
		"Shaders/shadow_rmiss.rmiss",
		"Shaders/ray_rchit.rchit"
	};
	myOpaqueShader = std::make_shared<Shader>(shaderPaths,
								 _ARRAYSIZE(shaderPaths),
								 theirVulkanFramework
	);

	// PIPELINE
	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps = {};
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 props = {};
	props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props.pNext = &rtProps;
	vkGetPhysicalDeviceProperties2(theirVulkanFramework.GetPhysicalDevice(), &props);

	RTPipelineBuilder builder;
	builder.AddDescriptorSet(theirSceneGlobals.GetGlobalsLayout());
	builder.AddDescriptorSet(theirImageHandler.GetSamplerSetLayout());
	builder.AddDescriptorSet(theirImageHandler.GetImageSetLayout());
	builder.AddDescriptorSet(theirAccStructHandler.GetInstanceStructuresLayout());
	builder.AddDescriptorSet(theirMeshHandler.GetMeshDataLayout());
	builder.AddShaderGroup(0, ShaderGroupType::RayGen);
	builder.AddShaderGroup(1, ShaderGroupType::Miss);
	builder.AddShaderGroup(2, ShaderGroupType::Miss);
	builder.AddShaderGroup(3, ShaderGroupType::ClosestHit);
	builder.AddShader(myOpaqueShader);
	VkResult result;
	std::tie(result, myPipeline) = builder.Construct(theirVulkanFramework.GetDevice(), rtProps);
	assert(!result && "failed creating ray tracing pipeline");
	
	size_t shaderProgramSize = rtProps.shaderGroupBaseAlignment;
	size_t shaderHandleAlignment = rtProps.shaderGroupHandleAlignment;
	size_t shaderHandleSize = rtProps.shaderGroupHandleSize;
	uint32_t alignedSize = AlignedSize(shaderHandleSize, shaderHandleAlignment);
	auto allocSub = theirBufferAllocator.Start();

	// SHADER BINDING TABLE
	myShaderBindingTables[0] =
		CreateShaderBindingTable(
			allocSub,
			alignedSize,
			0,
			1,
			myPipeline.pipeline);
	myShaderBindingTables[1] =
		CreateShaderBindingTable(
			allocSub,
			alignedSize,
			1,
			2,
			myPipeline.pipeline);
	myShaderBindingTables[2] =
		CreateShaderBindingTable(
			allocSub,
			alignedSize,
			3,
			1,
			myPipeline.pipeline);
	
	// STORE IMAGE
	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	theirImageHandler.LoadStorageImage(0, VK_FORMAT_R32G32B32A32_SFLOAT, w, h);

	// INSTANCE STRUCT
	VkAccelerationStructureInstanceKHR instance;
	instance.flags = NULL;
	instance.instanceCustomIndex = 0;
	instance.instanceShaderBindingTableRecordOffset = 0;
	instance.mask = 0xff;
	auto transform = glm::identity<Mat4rt>();
	instance.transform = *(VkTransformMatrixKHR*)&transform;
	instance.accelerationStructureReference = theirAccStructHandler[GeoStructID(0)].address;
	
	//RTInstances instances;
	myInstances.resize(MaxNumInstances, {instance});
	//myInstancesID = theirAccStructHandler.AddInstanceStructure(allocSub, myInstances);

	theirBufferAllocator.Queue(std::move(allocSub));
}

RTMeshRenderer::~RTMeshRenderer()
{
	vkDestroyPipeline(theirVulkanFramework.GetDevice(), myPipeline.pipeline, nullptr);
	vkDestroyPipelineLayout(theirVulkanFramework.GetDevice(), myPipeline.layout, nullptr);
	//vkDestroyBuffer(theirVulkanFramework.GetDevice(), mySBTBuffer, nullptr);
	//vkFreeMemory(theirVulkanFramework.GetDevice(), mySBTMemory, nullptr);
}

neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
RTMeshRenderer::RecordSubmit(
	uint32_t	swapchainImageIndex,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&
				waitSemaphores,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>&
				signalSemaphores)
{
	// ACQUIRE RENDER COMMAND BUFFER
	const auto& assembledWork = myWorkScheduler.AssembleScheduledWork();
	// UPDATE INSTANCE STRUCTURE
	myInstances.clear();
	for (auto& cmd : assembledWork)
	{
		RTInstances::value_type inst{};
		inst.accelerationStructureReference = theirAccStructHandler[cmd.geoID].address;
		inst.instanceCustomIndex = uint32_t(cmd.id);
		auto transform = glm::transpose(cmd.transform);
		inst.transform = *(VkTransformMatrixKHR*)&transform;
		inst.instanceShaderBindingTableRecordOffset = 0;
		inst.mask = 0xff;
		inst.flags = NULL;
		myInstances.emplace_back(inst);
	}

	// RECORD
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;

	beginInfo.pInheritanceInfo = nullptr;

	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
	}

	theirAccStructHandler.UpdateInstanceStructure(swapchainImageIndex, myInstancesID, myInstances);
	auto cmdBuffer = myCmdBuffers[swapchainImageIndex];
	auto resultBegin = vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, myPipeline.pipeline);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, myPipeline.layout, 0, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirImageHandler.BindSamplers(cmdBuffer, myPipeline.layout, 1, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirImageHandler.BindImages(swapchainImageIndex, cmdBuffer, myPipeline.layout, 2, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirAccStructHandler.BindInstanceStructures(swapchainImageIndex, cmdBuffer, myPipeline.layout, 3);
	theirMeshHandler.BindMeshData(swapchainImageIndex, cmdBuffer, myPipeline.layout, 4, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	VkStridedDeviceAddressRegionKHR callableStride = {};
	vkCmdTraceRays(
		cmdBuffer,
		&myShaderBindingTables[0].stride,
		&myShaderBindingTables[1].stride,
		&myShaderBindingTables[2].stride,
		&callableStride,
		uint32_t(w), uint32_t(h), 1);

	auto resultEnd = vkEndCommandBuffer(myCmdBuffers[swapchainImageIndex]);

	if (resultBegin || resultEnd)
	{
		LOG("mesh renderer failed recording");
		return {};
	}

	// FILL IN SUBMISSION
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;

	submitInfo.pWaitDstStageMask = myWaitStages.data();

	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.waitSemaphoreCount = waitSemaphores.size();
	submitInfo.pCommandBuffers = &myCmdBuffers[swapchainImageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores.data();
	submitInfo.signalSemaphoreCount = signalSemaphores.size();

	return {{myCmdBufferFences[swapchainImageIndex], submitInfo, VK_QUEUE_COMPUTE_BIT}};
}

std::vector<rflx::Features> RTMeshRenderer::GetImplementedFeatures() const
{
    return {rflx::Features::FEATURE_RAY_TRACING};
}

ShaderBindingTable
RTMeshRenderer::CreateShaderBindingTable(
	AllocationSubmissionID	allocSubID,
	size_t					handleSize,
	uint32_t				firstGroup,
	uint32_t				groupCount,
	VkPipeline				pipeline)
{
	std::vector<char> handleData(handleSize * groupCount);
	{
		auto result = vkGetRayTracingShaderGroupHandles(theirVulkanFramework.GetDevice(),
			pipeline,
			firstGroup,
			groupCount,
			handleSize * groupCount,
			handleData.data());
		int val = 0;
	}
	auto [result, buffer, memory] =
		theirBufferAllocator.CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			handleData.data(),
			handleData.size(),
			{QUEUE_FAMILY_COMPUTE, QUEUE_FAMILY_TRANSFER},
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	
	VkBufferDeviceAddressInfo addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = buffer;
	VkStridedDeviceAddressRegionKHR stride = {};
	stride.size = handleData.size();
	stride.stride = handleSize;
	stride.deviceAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);

	return {buffer, memory, stride};
}

uint32_t RTMeshRenderer::AlignedSize(uint32_t a, uint32_t b)
{
	return (a + b - 1) & ~(b - 1);
}
