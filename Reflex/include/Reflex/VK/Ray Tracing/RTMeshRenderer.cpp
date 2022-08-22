#include "pch.h"
#include "RTMeshRenderer.h"



#include "AccelerationStructureHandler.h"
#include "NVRayTracing.h"
#include "Reflex/VK/Image/ImageHandler.h"
#include "Reflex/VK/Memory/BufferAllocator.h"
#include "Reflex/VK/Mesh/MeshHandler.h"
#include "Reflex/VK/Scene/SceneGlobals.h"
#include "Reflex/VK/Shader/Shader.h"
#include "Reflex/VK/VulkanFramework.h"

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
	VkRayTracingShaderGroupCreateInfoNV rgenGrp{};
	rgenGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
	rgenGrp.pNext = nullptr;

	rgenGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
	rgenGrp.closestHitShader = VK_SHADER_UNUSED_NV;
	rgenGrp.anyHitShader = VK_SHADER_UNUSED_NV;
	rgenGrp.intersectionShader = VK_SHADER_UNUSED_NV;
	rgenGrp.generalShader = 0;

	VkRayTracingShaderGroupCreateInfoNV missGrp{};
	missGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
	missGrp.pNext = nullptr;

	missGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
	missGrp.closestHitShader = VK_SHADER_UNUSED_NV;
	missGrp.anyHitShader = VK_SHADER_UNUSED_NV;
	missGrp.intersectionShader = VK_SHADER_UNUSED_NV;
	missGrp.generalShader = 1;

	VkRayTracingShaderGroupCreateInfoNV shadowMissGrp{};
	shadowMissGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
	shadowMissGrp.pNext = nullptr;

	shadowMissGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_NV;
	shadowMissGrp.closestHitShader = VK_SHADER_UNUSED_NV;
	shadowMissGrp.anyHitShader = VK_SHADER_UNUSED_NV;
	shadowMissGrp.intersectionShader = VK_SHADER_UNUSED_NV;
	shadowMissGrp.generalShader = 2;

	VkRayTracingShaderGroupCreateInfoNV chitGrp{};
	chitGrp.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_NV;
	chitGrp.pNext = nullptr;

	chitGrp.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_NV;
	chitGrp.closestHitShader = 3;
	chitGrp.anyHitShader = VK_SHADER_UNUSED_NV;
	chitGrp.intersectionShader = VK_SHADER_UNUSED_NV;
	chitGrp.generalShader = VK_SHADER_UNUSED_NV;

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
	VkRayTracingPipelineCreateInfoNV rtPipelineInfo{};
	rtPipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_NV;
	rtPipelineInfo.pNext = nullptr;
	rtPipelineInfo.flags = NULL;

	auto [stages, numStages] = myOpaqueShader->Bind();
	rtPipelineInfo.pStages = stages.data();
	rtPipelineInfo.stageCount = numStages;

	myNumMissShaders = 2;
	myNumClosestHitShaders = 2;


	VkPhysicalDeviceRayTracingPropertiesNV rtProps{};
	rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PROPERTIES_NV;
	VkPhysicalDeviceProperties2 props{};
	props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	props.pNext = &rtProps;

	vkGetPhysicalDeviceProperties2(theirVulkanFramework.GetPhysicalDevice(), &props);

	rtPipelineInfo.maxRecursionDepth = rtProps.maxRecursionDepth;

	VkRayTracingShaderGroupCreateInfoNV shaderGroups[]
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
	failure = vkCreateRayTracingPipelines(theirVulkanFramework.GetDevice(), nullptr, 1, &rtPipelineInfo, nullptr, &myRTPipeLine);
	assert(!failure && "failed creating pipeline");

	// SBT
	myShaderProgramSize = rtProps.shaderGroupBaseAlignment;

	size_t sbtSize = myShaderProgramSize * ARRAYSIZE(shaderGroups);
	std::vector<char> shaderGrpHandles;
	shaderGrpHandles.resize(sbtSize);

	int offset = 0;
	for (int handleIndex = 0; handleIndex < ARRAYSIZE(shaderGroups); ++handleIndex)
	{
		vkGetRayTracingShaderGroupHandles(theirVulkanFramework.GetDevice(),
										   myRTPipeLine,
										   handleIndex,
										   1,
										   myShaderProgramSize,
										   shaderGrpHandles.data() + offset);
		offset += myShaderProgramSize;
	}


	auto allocSub = theirBufferAllocator.Start();
	std::tie(failure, myShaderBindingTable, mySBTMemory) =
		theirBufferAllocator.CreateBuffer(
			allocSub,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			shaderGrpHandles.data(),
			sbtSize,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			true);

	// STORE IMAGE
	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	theirImageHandler.LoadStorageImage(0, VK_FORMAT_R32G32B32A32_SFLOAT, w, h);

	// INSTANCE STRUCT
	RTInstances instances;
	instances.resize(MaxNumInstances);
	myInstancesID = theirAccStructHandler.AddInstanceStructure(allocSub, instances);

	theirBufferAllocator.Queue(std::move(allocSub));
}

RTMeshRenderer::~RTMeshRenderer()
{
	vkDestroyPipeline(theirVulkanFramework.GetDevice(), myRTPipeLine, nullptr);
	vkDestroyPipelineLayout(theirVulkanFramework.GetDevice(), myRTPipelineLayout, nullptr);
	vkDestroyBuffer(theirVulkanFramework.GetDevice(), myShaderBindingTable, nullptr);
	vkFreeMemory(theirVulkanFramework.GetDevice(), mySBTMemory, nullptr);

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
	RTInstances instances;
	for (auto& cmd : assembledWork)
	{
		uint64_t geoHandle{};
		auto geoStruct = theirAccStructHandler.GetGeometryStruct(cmd.id);
		auto result = vkGetAccelerationStructureHandle(theirVulkanFramework.GetDevice(), geoStruct, sizeof geoHandle, &geoHandle);
		assert(!result && "failed getting geo struct GPU handle");

		GeometryInstance inst{};
		inst.accelerationStructureHandle = geoHandle;
		inst.customID = uint32_t(cmd.id);
		inst.transform = glm::transpose(cmd.transform);
		inst.instanceOffset = 0;
		inst.mask = 0xff;
		inst.flags = NULL;
		instances.emplace_back(inst);
	}
	theirAccStructHandler.UpdateInstanceStructure(myInstancesID, instances);

	// RECORD
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = NULL;

	beginInfo.pInheritanceInfo = nullptr;

	while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), myCmdBufferFences[swapchainImageIndex]))
	{
	}
	auto resultBegin = vkBeginCommandBuffer(myCmdBuffers[swapchainImageIndex], &beginInfo);

	vkCmdBindPipeline(myCmdBuffers[swapchainImageIndex], VK_PIPELINE_BIND_POINT_RAY_TRACING_NV, myRTPipeLine);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(myCmdBuffers[swapchainImageIndex], myRTPipelineLayout, 0, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);
	theirImageHandler.BindSamplers(myCmdBuffers[swapchainImageIndex], myRTPipelineLayout, 1, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);
	theirImageHandler.BindImages(swapchainImageIndex, myCmdBuffers[swapchainImageIndex], myRTPipelineLayout, 2, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);
	theirAccStructHandler.BindInstanceStructures(myCmdBuffers[swapchainImageIndex], myRTPipelineLayout, 3);
	theirMeshHandler.BindMeshData(swapchainImageIndex, myCmdBuffers[swapchainImageIndex], myRTPipelineLayout, 4, VK_PIPELINE_BIND_POINT_RAY_TRACING_NV);

	// TRACE RAYS
	const uint32_t missOffset = myShaderProgramSize;
	const uint32_t missWidth = myShaderProgramSize * myNumMissShaders;
	const uint32_t chitOffset = missOffset + missWidth;

	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	vkCmdTraceRays(myCmdBuffers[swapchainImageIndex],
					myShaderBindingTable, 0,
					myShaderBindingTable, missOffset, myShaderProgramSize,
					myShaderBindingTable, chitOffset, myShaderProgramSize,
					myShaderBindingTable, 0, 0,
					w, h, 1);

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
