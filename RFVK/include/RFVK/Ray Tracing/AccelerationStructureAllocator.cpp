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
}

AccelerationStructureAllocator::~AccelerationStructureAllocator()
{
	{
		AllocatedAccelerationStructure allocAccStruct;
		while (myQueuedRequests.try_pop(allocAccStruct))
		{
			FreeAllocatedStructure(allocAccStruct);
		}
	}
	for (auto& [accStruct, allocAccStruct] : myAllocatedAccelerationStructures)
	{
		FreeAllocatedStructure(allocAccStruct);
	}
}

std::tuple<VkResult, VkAccelerationStructureKHR>
AccelerationStructureAllocator::RequestGeometryStructure(
	AllocationSubmissionID					allocSubID,
	const std::vector<struct MeshGeometry>&	meshes)
{

	
	std::vector<VkAccelerationStructureGeometryKHR> geometries;
	std::vector<VkAccelerationStructureBuildRangeInfoKHR> geoRangeInfos;
	std::vector<uint32_t> primitiveCounts;
	for (auto& mesh : meshes)
	{
		VkAccelerationStructureGeometryKHR asGeometry = {};
		asGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
		asGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
		asGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
		asGeometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
		asGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
		asGeometry.geometry.triangles.vertexData = {mesh.vertexAddress};
		asGeometry.geometry.triangles.maxVertex = mesh.numVertices;
		asGeometry.geometry.triangles.vertexStride = sizeof(Vertex3D);
		asGeometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
		asGeometry.geometry.triangles.indexData = {mesh.indexAddress};
		//asGeometry.geometry.triangles.transformData = {transAddress};
		geometries.emplace_back(asGeometry);

		VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
		rangeInfo.primitiveCount = mesh.numIndices / 3;
		geoRangeInfos.emplace_back(rangeInfo);

		primitiveCounts.emplace_back(mesh.numIndices / 3);
	}
	
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.pGeometries = geometries.data();
	buildInfo.geometryCount = uint32_t(geometries.size());

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
	sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizes(
		theirVulkanFramework.GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		primitiveCounts.data(),
		&sizeInfo);
	
	auto [resultScratch, scratchBuffer, scratchMemory] = 
		theirBufferAllocator.CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			nullptr,
			sizeInfo.buildScratchSize,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (resultScratch)
	{
		return {resultScratch, nullptr};
	}

	auto& allocSub = theirAllocationSubmitter[allocSubID];
	allocSub.AddResourceBuffer(scratchBuffer, scratchMemory);
	
	VkBufferDeviceAddressInfo addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = scratchBuffer;
	auto scratchAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);
	
	auto [resultAccStruct, accStructBuffer, accStructMemory] =
		theirBufferAllocator.CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT 
			| VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
			nullptr,
			sizeInfo.accelerationStructureSize,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (resultAccStruct)
	{
		return{resultAccStruct, nullptr};
	}

	VkAccelerationStructureCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.buffer = accStructBuffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	VkAccelerationStructureKHR geoStruct = nullptr;
	auto resultCreate = vkCreateAccelerationStructure(theirVulkanFramework.GetDevice(), &createInfo, nullptr, &geoStruct);
	if (resultCreate)
	{
		return { resultCreate, nullptr };
	}

	VkAccelerationStructureDeviceAddressInfoKHR accStructAddressInfo = {};
	accStructAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accStructAddressInfo.accelerationStructure = geoStruct;
	auto geoStructAddress = vkGetAccelerationStructureDeviceAddress(theirVulkanFramework.GetDevice(), &accStructAddressInfo);

	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.dstAccelerationStructure = geoStruct;
	buildInfo.scratchData = {scratchAddress};

	const auto cmdBuffer = allocSub.Record();
	auto pRangeInfos = geoRangeInfos.data();
	vkCmdBuildAccelerationStructures(cmdBuffer, 1, &buildInfo, &pRangeInfos);
	//VkMemoryBarrier barrier;
	//barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	//barrier.pNext = nullptr;
	//barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	//barrier.dstAccessMask =	VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	VkBufferMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	barrier.buffer = accStructBuffer;
	barrier.size = sizeInfo.accelerationStructureSize;
	
	vkCmdPipelineBarrier(cmdBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		NULL,
		0,
		nullptr,
		1,
		&barrier,
		0,
		nullptr);

	myQueuedRequests.push({
		geoStruct,
		{accStructBuffer, accStructMemory},
		{},
		{},
		0,
		geoStructAddress,
		0,
		0
	});

	return {VK_SUCCESS, geoStruct};
}

std::tuple<VkResult, VkAccelerationStructureKHR>
AccelerationStructureAllocator::RequestInstanceStructure(
	AllocationSubmissionID	allocSubID,
	const RTInstances&		instanceDesc)
{
	auto [resultInstances, instancesBuffer, instancesMemory] =
		theirBufferAllocator.CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR 
			| VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
			| VK_BUFFER_USAGE_TRANSFER_DST_BIT
			,
			instanceDesc.data(),
			uint32_t(instanceDesc.size() * sizeof RTInstances::value_type),
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (resultInstances)
	{
		return {resultInstances, nullptr};
	}
	
	VkBufferDeviceAddressInfo addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = instancesBuffer;
	auto instancesAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);

	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.data = {instancesAddress};
	geometry.geometry.instances.arrayOfPointers = false;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;
	
	const uint32_t primitiveCount = uint32_t(instanceDesc.size());
	VkAccelerationStructureBuildSizesInfoKHR sizesInfo = {};
	sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizes(
		theirVulkanFramework.GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&primitiveCount,
		&sizesInfo);
	
	auto [resultAccStruct, accStructBuffer, accStructMemory] =
		theirBufferAllocator.CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
			nullptr,
			sizesInfo.accelerationStructureSize,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (resultAccStruct)
	{
		return {resultAccStruct, nullptr};
	}

	VkAccelerationStructureCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.buffer = accStructBuffer;
	createInfo.size = sizesInfo.accelerationStructureSize;
	VkAccelerationStructureKHR instanceStructure = nullptr;
	auto resultCreate = vkCreateAccelerationStructure(theirVulkanFramework.GetDevice(), &createInfo, nullptr, &instanceStructure);
	if (resultCreate)
	{
		return{resultCreate, nullptr};
	}

	// BUILD
	auto [resultScratch, scratchBuffer, scratchMemory] =
		theirBufferAllocator.CreateBuffer(
			allocSubID,
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			nullptr,
			sizesInfo.buildScratchSize,
			myOwners,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (resultScratch)
	{
		return {resultScratch, nullptr};
	}
	addressInfo.buffer = scratchBuffer;
	auto scratchAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);
	
	BuildInstanceStructure(
		allocSubID,
		false,
		instanceDesc,
		instancesBuffer,
		instanceDesc.size() * sizeof VkAccelerationStructureInstanceKHR,
		instancesAddress,
		scratchAddress,
		instanceStructure);

	VkAccelerationStructureDeviceAddressInfoKHR accStructAddressInfo{};
	accStructAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	accStructAddressInfo.accelerationStructure = instanceStructure;
	auto instStructureAddress = vkGetAccelerationStructureDeviceAddress(theirVulkanFramework.GetDevice(), &accStructAddressInfo);

	myQueuedRequests.push({
		instanceStructure,
		{accStructBuffer, accStructMemory},
		{scratchBuffer, scratchMemory},
		{instancesBuffer, instancesMemory},
		instStructureAddress,
		scratchAddress,
		instancesAddress,
		instanceDesc.size() * sizeof RTInstances::value_type
	});
	
	return {VK_SUCCESS, instanceStructure};
}
void
AccelerationStructureAllocator::UpdateInstanceStructure(
	VkAccelerationStructureKHR	instanceStructure, 
	const RTInstances&			instanceDesc)
{
	if (!myAllocatedAccelerationStructures.contains(instanceStructure))
	{
		return;
	}
	auto& accStruct = myAllocatedAccelerationStructures[instanceStructure];
	auto allocSubID = theirAllocationSubmitter.StartAllocSubmission();
	
	BuildInstanceStructure(
		allocSubID,
		false,
		instanceDesc,
		accStruct.instancesBuffer.buffer,
		accStruct.instancesBufferSize,
		accStruct.instancesAddress,
		accStruct.scratchAddress,
		instanceStructure);
		
	theirAllocationSubmitter.QueueAllocSubmission(std::move(allocSubID));
}

void
AccelerationStructureAllocator::QueueDestroy(
	VkAccelerationStructureKHR accStruct, 
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
			FreeAllocatedStructure(allocAccStruct);
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

void
AccelerationStructureAllocator::FreeAllocatedStructure(
	const AllocatedAccelerationStructure& allocatedStructure) const
{
	vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocatedStructure.structureBuffer.buffer, nullptr);
	vkFreeMemory(theirVulkanFramework.GetDevice(), allocatedStructure.structureBuffer.memory, nullptr);
	vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocatedStructure.instancesBuffer.buffer, nullptr);
	vkFreeMemory(theirVulkanFramework.GetDevice(), allocatedStructure.instancesBuffer.memory, nullptr);
	vkDestroyBuffer(theirVulkanFramework.GetDevice(), allocatedStructure.scratchBuffer.buffer, nullptr);
	vkFreeMemory(theirVulkanFramework.GetDevice(), allocatedStructure.scratchBuffer.memory, nullptr);
	vkDestroyAccelerationStructure(theirVulkanFramework.GetDevice(), allocatedStructure.structure, nullptr);
}

void
AccelerationStructureAllocator::BuildInstanceStructure(
	AllocationSubmissionID		allocSubID,
	bool						update,
	const RTInstances&			instanceDesc,
	VkBuffer					instancesBuffer,
	size_t						instancesBufferSize,
	VkDeviceAddress				instancesBufferAddress,
	VkDeviceAddress				scratchBufferAddress,
	VkAccelerationStructureKHR	instanceStructure)
{
	auto& allocSub = theirAllocationSubmitter[allocSubID];
	
	if (!instanceDesc.empty())
	{
		VkBufferCopy copy;
		copy.size = instanceDesc.size() * sizeof RTInstances::value_type;
		copy.size = copy.size > instancesBufferSize ? instancesBufferSize : copy.size;
		copy.srcOffset = 0;
		copy.dstOffset = 0;

		auto [resultStaged, stagedBuffer] = CreateStagingBuffer(instanceDesc.data(), copy.size, myOwners.data(), myOwners.size());
		vkCmdCopyBuffer(allocSub.Record(), stagedBuffer.buffer, instancesBuffer, 1, &copy);

		allocSub.AddResourceBuffer(stagedBuffer.buffer, stagedBuffer.memory);
	}
	
	VkAccelerationStructureGeometryKHR geometry = {};
	geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	geometry.geometry.instances.data = {instancesBufferAddress};
	geometry.geometry.instances.arrayOfPointers = false;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
	buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &geometry;
	buildInfo.flags = 
		VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.srcAccelerationStructure = update ? instanceStructure : nullptr;
	buildInfo.dstAccelerationStructure = instanceStructure;
	buildInfo.scratchData = {scratchBufferAddress};

	VkAccelerationStructureBuildRangeInfoKHR buildRange = {};
	buildRange.primitiveCount = uint32_t(instanceDesc.size());
	std::vector ranges
	{
		buildRange
	};
	auto const pRanges = ranges.data();

	auto cmdBuffer = allocSub.Record();
	VkMemoryBarrier barrier;
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	barrier.pNext = nullptr;
	barrier.srcAccessMask =
		NULL;
	barrier.dstAccessMask =
		VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	vkCmdPipelineBarrier(cmdBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		NULL,
		1,
		&barrier,
		0,
		nullptr,
		0,
		nullptr);

	const uint32_t primitiveCount = uint32_t(instanceDesc.size());
	VkAccelerationStructureBuildSizesInfoKHR sizesInfo = {};
	sizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	vkGetAccelerationStructureBuildSizes(
		theirVulkanFramework.GetDevice(),
		VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
		&buildInfo,
		&primitiveCount,
		&sizesInfo);

	vkCmdBuildAccelerationStructures(cmdBuffer, 1, &buildInfo, &pRanges);
	/*barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
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
	vkCmdPipelineBarrier(cmdBuffer,
		VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
		VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
		NULL,
		1,
		&barrier,
		0,
		nullptr,
		0,
		nullptr);*/
}
