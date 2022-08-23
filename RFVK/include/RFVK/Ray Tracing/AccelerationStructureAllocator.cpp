#include "pch.h"
#include "AccelerationStructureAllocator.h"



#include "NVRayTracing.h"
#include "RFVK/Geometry/Vertex3D.h"
#include "RFVK/Memory/BufferAllocator.h"
#include "RFVK/Memory/ImmediateTransferrer.h"
#include "RFVK/Mesh/Mesh.h"
#include "RFVK/VulkanFramework.h"

AccelerationStructureAllocator::AccelerationStructureAllocator(
	VulkanFramework&		vulkanFramework,
	AllocationSubmitter&	allocationSubmitter,
	BufferAllocator&		bufferAllocator,
	ImmediateTransferrer&	immediateTransferrer,
	QueueFamilyIndex		transferFamilyIndex,
	QueueFamilyIndex		presentationFamilyIndex)
	: AllocatorBase(vulkanFramework, allocationSubmitter, immediateTransferrer, transferFamilyIndex)
	, theirBufferAllocator(bufferAllocator)
	, myOwners{transferFamilyIndex, presentationFamilyIndex}
	, myTransferFamilyIndex(transferFamilyIndex)
	, myPresentationFamilyIndex(presentationFamilyIndex)
{
	// INSTANCE STRUCTURE LIMIT CALCULATIONS
	VkAccelerationStructureNV instStructure;

	VkAccelerationStructureInfoNV accStructInfo{};
	accStructInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	accStructInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
	accStructInfo.pNext = nullptr;

	accStructInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
	accStructInfo.instanceCount = MaxNumInstances;

	accStructInfo.geometryCount = 0;
	accStructInfo.pGeometries = nullptr;

	VkAccelerationStructureCreateInfoNV createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
	createInfo.pNext = nullptr;

	createInfo.info = accStructInfo;
	createInfo.compactedSize = 0;

	VkResult failure{};
	failure = vkCreateAccelerationStructure(theirVulkanFramework.GetDevice(), &createInfo, nullptr, &instStructure);
	assert(!failure && "failed creating instance structure");

	// SCRATCH AND OBJ BUFFERS
	auto allocSub = theirBufferAllocator.Start();
	
	auto [memReqObj, memIndexObj] =
		GetMemReq(instStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV);
	myInstanceObjMaxSize = memReqObj.memoryRequirements.size;

	auto [memReqScratch, memIndexScratch] =
		GetMemReq(instStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV);
	myInstanceScratchSize = memReqScratch.memoryRequirements.size;
	std::tie(failure, myInstanceScratchBuffer, myInstanceScratchMemory) = theirBufferAllocator.CreateBuffer(	
																										allocSub,
																										VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
																										nullptr,
																										memReqScratch.memoryRequirements.size,
																										myOwners,
																										VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	assert(!failure && "failed creating scratch buffer");

	std::tie(failure, myInstanceDescBuffer, myInstanceDescMemory) = theirBufferAllocator.CreateBuffer(
																										allocSub,
																										VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
																										nullptr,
																										MaxNumInstances * sizeof GeometryInstance,
																										myOwners,
																										VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	myInstanceDescSize = MaxNumInstances * sizeof GeometryInstance;
	assert(!failure && "failed creating instance desc buffer");

	theirBufferAllocator.Queue(std::move(allocSub));
}

AccelerationStructureAllocator::~AccelerationStructureAllocator()
{
	{
		AllocatedAccelerationStructure allocAccStruct;
		while (myQueuedRequests.try_pop(allocAccStruct))
		{
			vkFreeMemory(theirVulkanFramework.GetDevice(), allocAccStruct.memory, nullptr);
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocAccStruct.buffer, nullptr);
			vkDestroyAccelerationStructure(theirVulkanFramework.GetDevice(), allocAccStruct.structure, nullptr);
		}
	}
	for (auto& [accStruct, allocAccStruct] : myAllocatedAccelerationStructures)
	{
		vkFreeMemory(theirVulkanFramework.GetDevice(), allocAccStruct.memory, nullptr);
		vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocAccStruct.buffer, nullptr);
		vkDestroyAccelerationStructure(theirVulkanFramework.GetDevice(), allocAccStruct.structure, nullptr);
	}
	
	vkDestroyBuffer(theirVulkanFramework.GetDevice(), myInstanceDescBuffer, nullptr);
	vkFreeMemory(theirVulkanFramework.GetDevice(), myInstanceDescMemory, nullptr);
	vkDestroyBuffer(theirVulkanFramework.GetDevice(), myInstanceScratchBuffer, nullptr);
	vkFreeMemory(theirVulkanFramework.GetDevice(), myInstanceScratchMemory, nullptr);
}

std::tuple<VkResult, VkAccelerationStructureNV>
AccelerationStructureAllocator::RequestGeometryStructure(
	AllocationSubmission&					allocSub,
	const std::vector<struct MeshGeometry>&	meshes)
{
	// ACCELERATION STRUCTURE 
	VkAccelerationStructureNV geoStructure{};

	std::vector<VkGeometryNV> geometries;

	for (auto&& mesh : meshes)
	{
		VkGeometryTrianglesNV tris{};
		tris.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NV;
		tris.pNext = nullptr;

		tris.indexCount = mesh.numIndices;
		tris.indexData = mesh.indexBuffer;
		tris.indexType = VK_INDEX_TYPE_UINT32;
		tris.indexOffset = 0;

		tris.vertexCount = mesh.numVertices;
		tris.vertexData = mesh.vertexBuffer;
		tris.vertexStride = sizeof Vertex3D;
		tris.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		tris.vertexOffset = 0;

		tris.transformData = nullptr;

		VkGeometryNV geo{};
		geo.sType = VK_STRUCTURE_TYPE_GEOMETRY_NV;
		geo.pNext = nullptr;

		geo.flags = VK_GEOMETRY_OPAQUE_BIT_NV;

		geo.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NV;

		geo.geometry.triangles = tris;
		geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NV;

		geometries.emplace_back(geo);
	}

	VkAccelerationStructureInfoNV accStructInfo{};
	accStructInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	accStructInfo.pNext = nullptr;
	accStructInfo.flags = NULL;

	accStructInfo.instanceCount = 0;
	accStructInfo.geometryCount = geometries.size();
	accStructInfo.pGeometries = geometries.data();
	accStructInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NV;

	VkAccelerationStructureCreateInfoNV  accStructCreateInfo{};
	accStructCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
	accStructCreateInfo.pNext = nullptr;

	accStructCreateInfo.compactedSize = 0;
	accStructCreateInfo.info = accStructInfo;

	auto resultAccStruct = vkCreateAccelerationStructure(theirVulkanFramework.GetDevice(), &accStructCreateInfo, nullptr, &geoStructure);
	if (resultAccStruct)
	{
		return {resultAccStruct, geoStructure};
	}

	// SCRATCH AND OBJ BUFFERS
	auto [memReqObj, memIndexObj] =
		GetMemReq(geoStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV);
	auto [memReqScratch, memIndexScratch] =
		GetMemReq(geoStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV);

	auto [resultScratch, scratchBuffer, scratchMem] =
		theirBufferAllocator.CreateBuffer(
			allocSub,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
			nullptr,
			memReqScratch.memoryRequirements.size,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);
	if (resultScratch)
	{
		return {resultScratch, geoStructure};
	}
	auto [resultObj, objBuffer, objMem] =
		theirBufferAllocator.CreateBuffer(
			allocSub,
			VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
			nullptr,
			memReqObj.memoryRequirements.size,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
		);;
	if (resultObj)
	{
		return {resultObj, geoStructure};
	}

	// BIND MEM TO ACC STRUCT
	VkBindAccelerationStructureMemoryInfoNV memInfo{};
	memInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
	memInfo.pNext = nullptr;

	memInfo.accelerationStructure = geoStructure;
	memInfo.deviceIndexCount = 0;
	memInfo.pDeviceIndices = nullptr;
	memInfo.memory = objMem;
	memInfo.memoryOffset = 0;

	auto resMemBind = vkBindAccelerationStructureMemory(theirVulkanFramework.GetDevice(), 1, &memInfo);
	if (resMemBind)
	{
		return {resMemBind, geoStructure};
	}

	// BUILD ACC STRUCT
	VkMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask =
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
	barrier.dstAccessMask =
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

	const auto& cmdBuffer = allocSub.Record();
	vkCmdBuildAccelerationStructure(cmdBuffer,
									 &accStructInfo,
									 nullptr,
									 0,
									 false,
									 geoStructure,
									 nullptr,
									 scratchBuffer,
									 0);
	vkCmdPipelineBarrier(cmdBuffer,
						  VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
						  VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
						  NULL,
						  1,
						  &barrier,
						  0,
						  nullptr,
						  0,
						  nullptr);
	

	allocSub.AddStagingBuffer({ scratchBuffer, scratchMem });
	//vkDestroyBuffer(theirVulkanFramework.GetDevice(), scratchBuffer, nullptr);
	//vkFreeMemory(theirVulkanFramework.GetDevice(), scratchMem, nullptr);
	myQueuedRequests.push({geoStructure, objBuffer, objMem});
	
	return {resultAccStruct, geoStructure};
}

std::tuple<VkResult, VkAccelerationStructureNV>
AccelerationStructureAllocator::RequestInstanceStructure(
	AllocationSubmission&	allocSub,
	const RTInstances&		instanceDesc)
{
	// ACCELERATION STRUCTURES
	VkAccelerationStructureNV instStructure;

	VkAccelerationStructureInfoNV accStructInfo{};
	accStructInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	accStructInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
	accStructInfo.pNext = nullptr;

	accStructInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
	accStructInfo.instanceCount = instanceDesc.size();

	accStructInfo.geometryCount = 0;
	accStructInfo.pGeometries = nullptr;

	VkAccelerationStructureCreateInfoNV createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NV;
	createInfo.pNext = nullptr;

	createInfo.info = accStructInfo;
	createInfo.compactedSize = 0;

	auto resultInstStruct = vkCreateAccelerationStructure(theirVulkanFramework.GetDevice(), &createInfo, nullptr, &instStructure);

	// SCRATCH AND OBJ BUFFERS
	auto [memReqObj, memIndexObj] =
		GetMemReq(instStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_OBJECT_NV);
	auto [memReqScratch, memIndexScratch] =
		GetMemReq(instStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_BUILD_SCRATCH_NV);
	assert(memReqScratch.memoryRequirements.size <= myInstanceScratchSize && "scratch mem req size larger than initially calculated max size");
	
	auto [resultObj, objBuffer, objMem] = theirBufferAllocator.CreateBuffer(
																				allocSub,
																				VK_BUFFER_USAGE_RAY_TRACING_BIT_NV,
																			 nullptr,
																			 myInstanceObjMaxSize,
																			 myOwners,
																			 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);;
	if (resultObj)
	{
		return {resultObj, instStructure};
	}

	// INSTANCE BUFFER
	size_t bufferWidth = instanceDesc.size() * sizeof GeometryInstance;

	void* mappedData;
	vkMapMemory(theirVulkanFramework.GetDevice(), myInstanceDescMemory, 0, bufferWidth, NULL, &mappedData);
	memcpy(mappedData, instanceDesc.data(), instanceDesc.size() * sizeof GeometryInstance);
	vkUnmapMemory(theirVulkanFramework.GetDevice(), myInstanceDescMemory);

	// BIND MEM TO ACC STRUCT
	VkBindAccelerationStructureMemoryInfoNV memInfo{};
	memInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NV;
	memInfo.pNext = nullptr;

	memInfo.accelerationStructure = instStructure;
	memInfo.deviceIndexCount = 0;
	memInfo.pDeviceIndices = nullptr;
	memInfo.memory = objMem;
	memInfo.memoryOffset = 0;

	auto resMemBind = vkBindAccelerationStructureMemory(theirVulkanFramework.GetDevice(), 1, &memInfo);
	if (resMemBind)
	{
		return {resMemBind, instStructure};
	}

	// BUILD ACC STRUCT
	VkMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask =
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;
	barrier.dstAccessMask =
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NV | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NV;

	const auto& cmdBuffer = allocSub.Record();
	vkCmdBuildAccelerationStructure(cmdBuffer,
		&accStructInfo,
		myInstanceDescBuffer, 0,
		false,
		instStructure,
		nullptr,
		myInstanceScratchBuffer, 0);
	vkCmdPipelineBarrier(cmdBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_NV,
		NULL,
		1,
		&barrier,
		0,
		nullptr,
		0,
		nullptr);

	myQueuedRequests.push({instStructure, objBuffer, objMem});

	return {resultInstStruct, instStructure};
}

VkBuffer
AccelerationStructureAllocator::GetUnderlyingBuffer(VkAccelerationStructureNV accStruct)
{
	return myAllocatedAccelerationStructures[accStruct].buffer;
}

void
AccelerationStructureAllocator::UpdateInstanceStructure(
	VkAccelerationStructureNV	instanceStructure, 
	const RTInstances&			instanceDesc)
{
	VkAccelerationStructureInfoNV accStructInfo{};
	accStructInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_INFO_NV;
	accStructInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_NV;
	accStructInfo.pNext = nullptr;

	accStructInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NV;
	accStructInfo.instanceCount = instanceDesc.size();

	accStructInfo.geometryCount = 0;
	accStructInfo.pGeometries = nullptr;

	auto [memReqScratch, memIndexScratch] =
		GetMemReq(instanceStructure,
				   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				   VK_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_TYPE_UPDATE_SCRATCH_NV);
	assert(memReqScratch.memoryRequirements.size <= myInstanceScratchSize && "scratch mem req size larger than initially calculated max size");

	void* mappedData;
	vkMapMemory(theirVulkanFramework.GetDevice(), myInstanceDescMemory, 0, myInstanceDescSize, NULL, &mappedData);
	memcpy(mappedData, instanceDesc.data(), instanceDesc.size() * sizeof GeometryInstance);
	vkUnmapMemory(theirVulkanFramework.GetDevice(), myInstanceDescMemory);

	AllocationSubmission allocSub = theirAllocationSubmitter.StartAllocSubmission();
	vkCmdBuildAccelerationStructure(
		allocSub.Record(),
		&accStructInfo,
		myInstanceDescBuffer, 0,
		false,
		instanceStructure,
		nullptr,
		myInstanceScratchBuffer, 0);
	theirAllocationSubmitter.QueueAllocSubmission(std::move(allocSub));
	
}

void
AccelerationStructureAllocator::QueueDestroy(
	VkAccelerationStructureNV accStruct, 
	std::shared_ptr<std::counting_semaphore<NumSwapchainImages>> waitSignal)
{
	myQueuedDestroys.push({accStruct, 0, waitSignal});
}

void
AccelerationStructureAllocator::DoCleanUp(
	int limit)
{
	{
		int count = 0;
		AllocatedAccelerationStructure allocAccStruct = {};
		while (myQueuedRequests.try_pop(allocAccStruct)
			&& count++ < limit)
		{
			myAllocatedAccelerationStructures[allocAccStruct.structure] = allocAccStruct;
		}
	}
	{
		QueuedAccStructDestroy queuedDestroy;
		int count = 0;

		while (myQueuedDestroys.try_pop(queuedDestroy)
			&& count++ < limit)
		{
			if (!myAllocatedAccelerationStructures.contains(queuedDestroy.structure))
			{
				// Requested image may not be popped from queue yet
				myFailedDestroys.emplace_back(queuedDestroy);
				continue;
			}
			if (!SemaphoreWait(*queuedDestroy.waitSignal))
			{
				myFailedDestroys.emplace_back(queuedDestroy);
				continue;
			}
			auto allocAccStruct = myAllocatedAccelerationStructures[queuedDestroy.structure];
			vkFreeMemory(theirVulkanFramework.GetDevice(), allocAccStruct.memory, nullptr);
			vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocAccStruct.buffer, nullptr);
			vkDestroyAccelerationStructure(theirVulkanFramework.GetDevice(), allocAccStruct.structure, nullptr);
			myAllocatedAccelerationStructures.erase(queuedDestroy.structure);
		}
		for (auto&& failedDestroy : myFailedDestroys)
		{
			failedDestroy.tries++;
			myQueuedDestroys.push(failedDestroy);
		}
		myFailedDestroys.clear();
	}
}

BufferAllocator& 
AccelerationStructureAllocator::GetBufferAllocator() const
{
	return theirBufferAllocator;
}

std::vector<QueueFamilyIndex>
AccelerationStructureAllocator::GetOwners() const
{
	return myOwners;
}

std::tuple<VkMemoryRequirements2, MemTypeIndex>
AccelerationStructureAllocator::GetMemReq(
	VkAccelerationStructureNV						accStruct,
	VkMemoryPropertyFlags							memPropFlags,
	VkAccelerationStructureMemoryRequirementsTypeNV accMemType)
{
	VkAccelerationStructureMemoryRequirementsInfoNV accMemReqInfo{};
	accMemReqInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NV;
	accMemReqInfo.pNext = nullptr;
	accMemReqInfo.accelerationStructure = accStruct;
	accMemReqInfo.type = accMemType;

	VkMemoryRequirements2 memReq2{};
	vkGetAccelerationStructureMemoryRequirements(theirVulkanFramework.GetDevice(), &accMemReqInfo, &memReq2);

	MemTypeIndex chosenIndex = UINT_MAX;
	for (auto& index : myMemoryTypes[memPropFlags])
	{
		if ((1 << index) & memReq2.memoryRequirements.memoryTypeBits)
		{
			chosenIndex = index;
			break;
		}
	}

	return {memReq2, chosenIndex};
}