#include "pch.h"
#include "RayTracer.h"

#include "RFVK/Image/ImageHandler.h"
#include "RFVK/Memory/BufferAllocator.h"
#include "RFVK/Mesh/MeshHandler.h"
#include "RFVK/Scene/SceneGlobals.h"
#include "RFVK/Shader/Shader.h"
#include "RFVK/VulkanFramework.h"
#include "RFVK/Ray Tracing/AccelerationStructureHandler.h"
#include "RFVK/Ray Tracing/RTPipelineBuilder.h"


RayTracer::RayTracer(
	VulkanFramework&				vulkanFramework,
	MeshHandler&					meshHandler,
	ImageHandler&					imageHandler,
	SceneGlobals&					sceneGlobals,
	BufferAllocator&				bufferAllocator,
	AccelerationStructureHandler&	accStructHandler,
	const GBuffer&					gBuffer,
	QueueFamilyIndices				familyIndices)
	: theirVulkanFramework(vulkanFramework)
	, theirMeshHandler(meshHandler)
	, theirImageHandler(imageHandler)
	, theirSceneGlobals(sceneGlobals)
	, theirBufferAllocator(bufferAllocator)
	, theirAccStructHandler(accStructHandler)
	, myGBuffer(gBuffer)
{
	// SHADERS
	char shaderPaths[][128]
	{
		"Shaders/drt_rgen.rgen",
		"Shaders/ray_rmiss.rmiss",
		"Shaders/shadow_rmiss.rmiss",
		"Shaders/drt_rchit.rchit"
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
	builder.AddDescriptorSet(myGBuffer.layout);
	
	builder.AddShaderGroup(0, ShaderGroupType::RayGen);
	builder.AddShaderGroup(1, ShaderGroupType::Miss);
	builder.AddShaderGroup(2, ShaderGroupType::Miss);
	builder.AddShaderGroup(3, ShaderGroupType::ClosestHit);
	builder.AddShader(myOpaqueShader);
	VkResult result;
	std::tie(result, myPipeline) = builder.Construct(theirVulkanFramework.GetDevice(), rtProps);
	assert(!result && "failed creating ray tracing pipeline");

	uint32_t shaderProgramSize = rtProps.shaderGroupBaseAlignment;
	uint32_t shaderHandleAlignment = rtProps.shaderGroupHandleAlignment;
	uint32_t shaderHandleSize = rtProps.shaderGroupHandleSize;
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
	myInstancesID = theirAccStructHandler.AddInstanceStructure(allocSub, myInstances);

	theirBufferAllocator.Queue(std::move(allocSub));
}

RayTracer::~RayTracer()
{
	//vkDestroyPipeline(theirVulkanFramework.GetDevice(), myPipeline.pipeline, nullptr);
	//vkDestroyPipelineLayout(theirVulkanFramework.GetDevice(), myPipeline.layout, nullptr);
	//vkDestroyBuffer(theirVulkanFramework.GetDevice(), mySBTBuffer, nullptr);
	//vkFreeMemory(theirVulkanFramework.GetDevice(), mySBTMemory, nullptr);
}

void
RayTracer::Record(
	int				swapchainIndex,
	VkCommandBuffer	cmdBuffer,
	const MeshRenderSchedule::AssembledWorkType&
					assembledWork)
{
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

	theirAccStructHandler.UpdateInstanceStructure(swapchainIndex, myInstancesID, myInstances);

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, myPipeline.pipeline);

	// DESCRIPTORS
	theirSceneGlobals.BindGlobals(cmdBuffer, myPipeline.layout, 0, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirImageHandler.BindSamplers(cmdBuffer, myPipeline.layout, 1, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirImageHandler.BindImages(swapchainIndex, cmdBuffer, myPipeline.layout, 2, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	theirAccStructHandler.BindInstanceStructures(swapchainIndex, cmdBuffer, myPipeline.layout, 3);
	theirMeshHandler.BindMeshData(swapchainIndex, cmdBuffer, myPipeline.layout, 4, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR);
	vkCmdBindDescriptorSets(
		cmdBuffer,
		VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
		myPipeline.layout,
		5,
		1,
		&myGBuffer.set,
		0,
		nullptr);

	auto [w, h] = theirVulkanFramework.GetTargetResolution();
	VkStridedDeviceAddressRegionKHR callableStride = {};
	vkCmdTraceRays(
		cmdBuffer,
		&myShaderBindingTables[0].stride,
		&myShaderBindingTables[1].stride,
		&myShaderBindingTables[2].stride,
		&callableStride,
		uint32_t(w), uint32_t(h), 1);
}

ShaderBindingTable
RayTracer::CreateShaderBindingTable(
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

uint32_t RayTracer::AlignedSize(uint32_t a, uint32_t b)
{
	return (a + b - 1) & ~(b - 1);
}
