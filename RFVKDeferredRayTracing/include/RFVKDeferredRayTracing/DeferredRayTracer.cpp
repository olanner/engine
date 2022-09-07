#include "pch.h"
#include "DeferredRayTracer.h"

#include "DeferredGeoRenderer.h"
#include "RayTracer.h"
#include "RFVK/VulkanFramework.h"
#include "RFVK/Image/ImageHandler.h"
#include "RFVK/Memory/ImageAllocator.h"

DeferredRayTracer::DeferredRayTracer(
	VulkanFramework&	vulkanFramework, 
	UniformHandler&		uniformHandler, 
	MeshHandler&		meshHandler, 
	ImageHandler&		imageHandler, 
	SceneGlobals&		sceneGlobals, 
	RenderPassFactory&	renderPassFactory,
	BufferAllocator&	bufferAllocator,
	AccelerationStructureHandler&
						accStructHandler,
	QueueFamilyIndices	familyIndices)
	: theirVulkanFramework(vulkanFramework)
{
	myGeoWaitStages.fill(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
	myRTWaitStages.fill(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR);

	// GBUFFER
	auto& imageAllocator = imageHandler.GetImageAllocator();
	auto allocSubID = imageAllocator.Start();
	auto [sw, sh] = vulkanFramework.GetTargetResolution();

	ImageRequestInfo imageInfo = {};
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.width = sw;
	imageInfo.height = sh;
	imageInfo.layout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo.targetPipelineStage = 
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
		| VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
	imageInfo.usage =
		VK_IMAGE_USAGE_STORAGE_BIT
		| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	imageInfo.owners = {
		familyIndices[QUEUE_FAMILY_TRANSFER],
		familyIndices[QUEUE_FAMILY_COMPUTE],
		familyIndices[QUEUE_FAMILY_GRAPHICS],
	};
	
	VkResult result;
	std::tie(result, myGBuffer.albedo) =
		imageAllocator.RequestImage2D(allocSubID,nullptr,0,imageInfo);
	imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	std::tie(result, myGBuffer.normal) =
		imageAllocator.RequestImage2D(allocSubID, nullptr, 0, imageInfo);
	std::tie(result, myGBuffer.position) =
		imageAllocator.RequestImage2D(allocSubID, nullptr, 0, imageInfo);
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	std::tie(result, myGBuffer.material) =
		imageAllocator.RequestImage2D(allocSubID, nullptr, 0, imageInfo);

	
	// GBUFFER DESCRIPTORS
	VkDescriptorPoolSize poolSize;
	poolSize.descriptorCount = 4;
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.maxSets = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.poolSizeCount = 1;

	result = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!result && "failed creating descriptor pool");

	VkDescriptorSetLayoutBinding albBinding = {};
	albBinding.binding = 0;
	albBinding.descriptorCount = 1;
	albBinding.stageFlags = VK_SHADER_STAGE_ALL;
	albBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	VkDescriptorSetLayoutBinding posBinding = albBinding;
	posBinding.binding = 1;
	VkDescriptorSetLayoutBinding nrmBinding = albBinding;
	nrmBinding.binding = 2;
	VkDescriptorSetLayoutBinding matBinding = albBinding;
	matBinding.binding = 3;

	std::array bindings
	{
		albBinding,
		posBinding,
		nrmBinding,
		matBinding
	};
	
	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pBindings = bindings.data();
	layoutInfo.bindingCount = bindings.size();

	result = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &myGBuffer.layout);
	assert(!result && "failed creating descriptor set layout");

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = myDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &myGBuffer.layout;

	result = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &myGBuffer.set);
	assert(!result && "failed allocating descriptor set");

	// WRITE DESCRIPTOR SETS
	VkDescriptorImageInfo imageDescInfo = {};
	imageDescInfo.imageLayout = imageInfo.layout;
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	write.dstSet = myGBuffer.set;
	write.descriptorCount = 1;
	write.pImageInfo = &imageDescInfo;

	imageDescInfo.imageView = myGBuffer.albedo;
	write.dstBinding = 0;
	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	
	imageDescInfo.imageView = myGBuffer.position;
	write.dstBinding = 1;
	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	
	imageDescInfo.imageView = myGBuffer.normal;
	write.dstBinding = 2;
	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	
	imageDescInfo.imageView = myGBuffer.material;
	write.dstBinding = 3;
	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &write, 0, nullptr);
	
	// CMD BUFFERS AND FENCES
	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; swapchainIndex++)
	{
		VkFenceCreateInfo fenceInfo = {};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
		auto result = vkCreateFence(theirVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myGeoCmdBufferFences[swapchainIndex]);
		assert(!result && "failed creating fence");
		result = vkCreateFence(theirVulkanFramework.GetDevice(), &fenceInfo, nullptr, &myRTCmdBufferFences[swapchainIndex]);
		assert(!result && "failed creating fence");

		std::tie(result, myGeoCmdBuffers[swapchainIndex]) = theirVulkanFramework.RequestCommandBuffer(familyIndices[QUEUE_FAMILY_GRAPHICS]);
		assert(!result && "failed requesting command buffer");
		std::tie(result, myRTCmdBuffers[swapchainIndex]) = theirVulkanFramework.RequestCommandBuffer(familyIndices[QUEUE_FAMILY_COMPUTE]);
		assert(!result && "failed requesting command buffer");
	}

	// SUB WORKERS
	myGeoRenderer = std::make_shared<DeferredGeoRenderer>(
		vulkanFramework,
		uniformHandler,
		meshHandler,
		imageHandler,
		sceneGlobals,
		renderPassFactory,
		myGBuffer,
		familyIndices);
	myRayTracer = std::make_shared<RayTracer>(
		vulkanFramework,
		meshHandler,
		imageHandler,
		sceneGlobals,
		bufferAllocator,
		accStructHandler,
		myGBuffer,
		familyIndices);
}

DeferredRayTracer::~DeferredRayTracer()
{
}

neat::static_vector<WorkerSubmission, MaxWorkerSubmissions>
DeferredRayTracer::RecordSubmit(
	uint32_t swapchainImageIndex,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& waitSemaphores,
	const neat::static_vector<VkSemaphore, MaxWorkerSubmissions>& signalSemaphores)
{
	const auto assembledWork = myWorkScheduler.AssembleScheduledWork();
	
	mySubmissions.clear();
	{
		auto& fence = myGeoCmdBufferFences[swapchainImageIndex];
		while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), fence))
		{
		}
		auto& cmdBuffer = myGeoCmdBuffers[swapchainImageIndex];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		myGeoRenderer->Record(swapchainImageIndex, cmdBuffer, assembledWork);
		vkEndCommandBuffer(cmdBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;
		submitInfo.pWaitDstStageMask = myGeoWaitStages.data();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.waitSemaphoreCount = waitSemaphores.size();
		submitInfo.pSignalSemaphores = &signalSemaphores[0];
		submitInfo.signalSemaphoreCount = 1;

		mySubmissions.emplace_back(WorkerSubmission{
				fence,
				submitInfo,
				VK_QUEUE_GRAPHICS_BIT});
	}
	{
		auto& fence = myRTCmdBufferFences[swapchainImageIndex];
		while (vkGetFenceStatus(theirVulkanFramework.GetDevice(), fence))
		{
		}
		auto& cmdBuffer = myRTCmdBuffers[swapchainImageIndex];

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		vkBeginCommandBuffer(cmdBuffer, &beginInfo);
		myRayTracer->Record(swapchainImageIndex, cmdBuffer, assembledWork);
		vkEndCommandBuffer(cmdBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer;
		submitInfo.pWaitDstStageMask = myRTWaitStages.data();
		submitInfo.pWaitSemaphores = waitSemaphores.data();
		submitInfo.waitSemaphoreCount = 0;
		submitInfo.pSignalSemaphores = &signalSemaphores[1];
		submitInfo.signalSemaphoreCount = 1;

		mySubmissions.emplace_back(WorkerSubmission{
				fence,
				submitInfo,
				VK_QUEUE_COMPUTE_BIT});
	}
	
    return mySubmissions;
}

std::vector<rflx::Features> DeferredRayTracer::GetImplementedFeatures() const
{
	return {rflx::Features::FEATURE_RAY_TRACING};
}

std::array<VkFence, NumSwapchainImages> DeferredRayTracer::GetFences()
{
	return myGeoCmdBufferFences;
}
