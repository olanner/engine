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

RTMeshRenderer::RTMeshRenderer(
	VulkanFramework&				vulkanFramework,
	UniformHandler&					uniformHandler,
	MeshHandler&					meshHandler,
	ImageHandler&					imageHandler,
	SceneGlobals&					sceneGlobals,
	BufferAllocator&				bufferAllocator,
	AccelerationStructureHandler&	accStructHandler,
	QueueFamilyIndex				cmdBufferFamily,
	QueueFamilyIndex				transferFamily)
	: MeshRendererBase(vulkanFramework,
					  uniformHandler,
					  meshHandler,
					  imageHandler,
					  sceneGlobals,
					  cmdBufferFamily)
	, theirBufferAllocator(bufferAllocator)
	, theirAccStructHandler(accStructHandler)
	, myOwners{transferFamily, cmdBufferFamily}
{
	// SHADERS
	char shaderPaths[][128]
	{
		"Shaders/ray_rgen.rgen",
		"Shaders/ray_rmiss.rmiss",
		"Shaders/shadow_rmiss.rmiss",
		"Shaders/ray_rchit.rchit"
	};
	myOpaqueShader = new Shader(shaderPaths,
								 _ARRAYSIZE(shaderPaths),
								 theirVulkanFramework
	);
	
	// SHADER GROUPS
	VkRayTracingShaderGroupCreateInfoKHR rgenGrp{};
	rgenGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rgenGrp.pNext = nullptr;

	rgenGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rgenGrp.closestHitShader = VK_SHADER_UNUSED_KHR;
	rgenGrp.anyHitShader = VK_SHADER_UNUSED_KHR;
	rgenGrp.intersectionShader = VK_SHADER_UNUSED_KHR;
	rgenGrp.generalShader = 0;

	VkRayTracingShaderGroupCreateInfoKHR missGrp{};
	missGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	missGrp.pNext = nullptr;

	missGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	missGrp.closestHitShader = VK_SHADER_UNUSED_KHR;
	missGrp.anyHitShader = VK_SHADER_UNUSED_KHR;
	missGrp.intersectionShader = VK_SHADER_UNUSED_KHR;
	missGrp.generalShader = 1;

	VkRayTracingShaderGroupCreateInfoKHR shadowMissGrp{};
	shadowMissGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	shadowMissGrp.pNext = nullptr;

	shadowMissGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	shadowMissGrp.closestHitShader = VK_SHADER_UNUSED_KHR;
	shadowMissGrp.anyHitShader = VK_SHADER_UNUSED_KHR;
	shadowMissGrp.intersectionShader = VK_SHADER_UNUSED_KHR;
	shadowMissGrp.generalShader = 2;

	VkRayTracingShaderGroupCreateInfoKHR chitGrp{};
	chitGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	chitGrp.pNext = nullptr;

	chitGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	chitGrp.closestHitShader = 3;
	chitGrp.anyHitShader = VK_SHADER_UNUSED_KHR;
	chitGrp.intersectionShader = VK_SHADER_UNUSED_KHR;
	chitGrp.generalShader = VK_SHADER_UNUSED_KHR;

	// LAYOUT
	VkPipelineLayoutCreateInfo layoutCreateInfo{};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pNext = nullptr;
	layoutCreateInfo.flags = NULL;
	VkDescriptorSetLayout layouts[]
	{
		theirSceneGlobals.GetGlobalsLayout(),
		theirImageHandler.GetSamplerSetLayout(),
		theirImageHandler.GetImageSetLayout(),
		theirAccStructHandler.GetInstanceStructuresLayout(),
		theirMeshHandler.GetMeshDataLayout(),
	};
	layoutCreateInfo.pushConstantRangeCount = 0;
	layoutCreateInfo.pPushConstantRanges = nullptr;
	layoutCreateInfo.setLayoutCount = ARRAYSIZE(layouts);
	layoutCreateInfo.pSetLayouts = layouts;

	// PIPELINE
	VkRayTracingPipelineCreateInfoKHR rtPipelineInfo{};
	rtPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rtPipelineInfo.pNext = nullptr;
	rtPipelineInfo.flags = NULL;

	auto [stages, numStages] = myOpaqueShader->Bind();
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.stageCount = numStages;

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	VkPhysicalDeviceProperties2 props{};
	props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props.pNext = &rtProps;

	vkGetPhysicalDeviceProperties2(theirVulkanFramework.GetPhysicalDevice(), &props);

	rtPipelineInfo.maxPipelineRayRecursionDepth = rtProps.maxRayRecursionDepth;

	VkRayTracingShaderGroupCreateInfoKHR shaderGroups[]
	{
		rgenGrp,
		missGrp,
		shadowMissGrp,
		chitGrp,
	};
	rtPipelineInfo.pGroups = shaderGroups;
	rtPipelineInfo.groupCount = ARRAYSIZE(shaderGroups);


	auto failure = vkCreatePipelineLayout(theirVulkanFramework.GetDevice(), &layoutCreateInfo, nullptr, &myRTPipelineLayout);
	rtPipelineInfo.layout = myRTPipelineLayout;
	vkCreateRayTracingPipelines(
		theirVulkanFramework.GetDevice(),
		nullptr, 
		nullptr,
		1, 
		&rtPipelineInfo, 
		nullptr, 
		&myRTPipeLine);
	assert(!failure && "failed creating pipeline");

	// SBTs
	size_t shaderProgramSize = rtProps.shaderGroupBaseAlignment;
	size_t shaderHandleAlignment = rtProps.shaderGroupHandleAlignment;
	size_t shaderHandleSize = rtProps.shaderGroupHandleSize;
	uint32_t alignedSize = AlignedSize(shaderHandleSize, shaderHandleAlignment);
	auto allocSub = theirBufferAllocator.Start();

	myShaderBindingTables[0] =
		CreateShaderBindingTable(
			allocSub,
			alignedSize,
			0,
			1,
			myRTPipeLine);
	myShaderBindingTables[1] =
		CreateShaderBindingTable(
			allocSub,
			alignedSize,
			1,
			2,
			myRTPipeLine);
	myShaderBindingTables[2] =
		CreateShaderBindingTable(
			allocSub,
			alignedSize,
			3,
			1,
			myRTPipeLine);
	
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
	myInstancesID = theirAccStructHandler.AddInstanceStructure(allocSub, myInstances);

	theirBufferAllocator.Queue(std::move(allocSub));
}

RTMeshRenderer::~RTMeshRenderer()
{
	vkDestroyPipeline(theirVulkanFramework.GetDevice(), myRTPipeLine, nullptr);
	vkDestroyPipelineLayout(theirVulkanFramework.GetDevice(), myRTPipelineLayout, nullptr);
	//vkDestroyBuffer(theirVulkanFramework.GetDevice(), mySBTBuffer, nullptr);
	//vkFreeMemory(theirVulkanFramework.GetDevice(), mySBTMemory, nullptr);

	SAFE_DELETE(myOpaqueShader);
}

std::tuple<VkSubmitInfo, VkFence>
RTMeshRenderer::RecordSubmit(
	uint32_t				swapchainImageIndex,
	VkSemaphore*			waitSemaphores,
	uint32_t				numWaitSemaphores,
	VkPipelineStageFlags*	waitPipelineStages,
	uint32_t				numWaitStages,
	VkSemaphore*			signalSemaphore)
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

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, myRTPipeLine);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, myRTPipelineLayout, 0, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirImageHandler.BindSamplers(cmdBuffer, myRTPipelineLayout, 1, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirImageHandler.BindImages(swapchainImageIndex, cmdBuffer, myRTPipelineLayout, 2, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirAccStructHandler.BindInstanceStructures(swapchainImageIndex, cmdBuffer, myRTPipelineLayout, 3);
	theirMeshHandler.BindMeshData(swapchainImageIndex, cmdBuffer, myRTPipelineLayout, 4, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);

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

	submitInfo.pWaitDstStageMask = waitPipelineStages;

	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.waitSemaphoreCount = numWaitSemaphores;
	submitInfo.pCommandBuffers = &myCmdBuffers[swapchainImageIndex];
	submitInfo.commandBufferCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphore;
	submitInfo.signalSemaphoreCount = 1;

	return {submitInfo, myCmdBufferFences[swapchainImageIndex]};
}

std::vector<rflx::Features> RTMeshRenderer::GetImplementedFeatures() const
{
    return {rflx::Features::FEATURE_RAY_TRACING};
}

ShaderBindingTable
RTMeshRenderer::CreateShaderBindingTable(
	AllocationSubmission&		allocSub,
	size_t						handleSize,
	uint32_t					firstGroup,
	uint32_t					groupCount,
	VkPipeline)
{
	std::vector<char> handleData(handleSize * groupCount);
	{
		auto result = vkGetRayTracingShaderGroupHandles(theirVulkanFramework.GetDevice(),
			myRTPipeLine,
			firstGroup,
			groupCount,
			handleSize * groupCount,
			handleData.data());
		int val = 0;
	}
	auto [result, buffer, memory] =
		theirBufferAllocator.CreateBuffer(
			allocSub,
			VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			handleData.data(),
			handleData.size(),
			myOwners,
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
