#include "pch.h"
#include "AllocatorBase.h"

#include "RFVK/Debug/DebugUtils.h"
#include "RFVK/VulkanFramework.h"

AllocatorBase::AllocatorBase(
	VulkanFramework&			vulkanFramework,
	AllocationSubmitter&		allocationSubmitter,
	class ImmediateTransferrer& immediateTransferrer,
	QueueFamilyIndex			transferFamilyIndex)
	: theirVulkanFramework(vulkanFramework)
	, theirAllocationSubmitter(allocationSubmitter)
	, myTransferFamily(transferFamilyIndex)
{


	// ENUMERATE PHYSICAL DEVICE MEMORY
	myPhysicalDeviceMemProperties = theirVulkanFramework.GetPhysicalDeviceMemProps();

	for (MemTypeIndex typeIndex = 0; typeIndex < myPhysicalDeviceMemProperties.memoryTypeCount; typeIndex++)
	{
		constexpr int memPropFlagMax = 128;
		for (VkMemoryPropertyFlags a = 1; a <= memPropFlagMax; a = a << 1)
		{
			if (myPhysicalDeviceMemProperties.memoryTypes[typeIndex].propertyFlags & a)
			{
				myMemoryTypes[a].emplace_back(typeIndex);
			}
		}
		myMemoryTypes[myPhysicalDeviceMemProperties.memoryTypes[typeIndex].propertyFlags].emplace_back(typeIndex);
	}
}

AllocatorBase::~AllocatorBase()
{
}

AllocationSubmissionID
AllocatorBase::Start() const
{
	return theirAllocationSubmitter.StartAllocSubmission();
}

AllocationSubmissionID
AllocatorBase::Start(
	neat::ThreadID threadID) const
{
	return theirAllocationSubmitter.StartAllocSubmission(threadID);
}

void
AllocatorBase::Queue(
	AllocationSubmissionID&& allocationSubmissionID) const
{
	return theirAllocationSubmitter.QueueAllocSubmission(std::move(allocationSubmissionID));
}

AllocationSubmission& 
AllocatorBase::GetAllocationSubmission(
	AllocationSubmissionID allocationSubmissionID) const
{
	return theirAllocationSubmitter[allocationSubmissionID];
}

std::optional<AllocationSubmissionID>
AllocationSubmitter::AcquireNextAllocSubmission()
{
	if (AllocationSubmissionID allocSub; myQueuedSubmissions.try_pop(allocSub))
	{
		return std::optional(std::move(allocSub));
	}
	return std::nullopt;
}

void
AllocationSubmitter::QueueRelease(
	AllocationSubmissionID&& allocationSubmissionID)
{
	myQueuedReleases.push(std::move(allocationSubmissionID));
}

void
AllocationSubmitter::TryReleasing()
{
	AllocationSubmissionID allocSubID;
	int count = 0;
	constexpr int cMaxRelease = 128;
	while (myQueuedReleases.try_pop(allocSubID)
		&& count++ < cMaxRelease)
	{
		auto& allocSub = myAllocationSubmissions[int(allocSubID)];
		const auto threadID =  allocSub.GetOwningThread();
		const auto [success, cmdBuffer] = allocSub.Release();
		if (!success)
		{
			myQueuedReleases.push(std::move(allocSubID));
			continue;
		}
		myAllocSubIDs.ReturnID(allocSubID);
		//myQueuedCommandBufferDeallocs[threadID].push(cmdBuffer);
		myCommandBufferQueue[threadID].push(cmdBuffer);
	}
}

void
AllocationSubmitter::FreeUsedCommandBuffers(
	neat::ThreadID	threadID,
	int				maxCount)
{
	return;
	const auto cmdPool = myCommandPools[threadID];
	VkCommandBuffer cmdBuffer = nullptr;
	int count = 0;
	while (myQueuedCommandBufferDeallocs[threadID].try_pop(cmdBuffer)
		&& count++ < maxCount)
	{
		vkFreeCommandBuffers(
			theirVulkanFramework.GetDevice(),
			cmdPool,
			1,
			&cmdBuffer);
	}
}

AllocationSubmission& 
AllocationSubmitter::operator[](
	AllocationSubmissionID id)
{
	return myAllocationSubmissions[int(id)];
}

AllocationSubmissionID
AllocationSubmitter::StartAllocSubmission()
{
	auto id = myAllocSubIDs.FetchFreeID();
	if (BAD_ID(id))
	{
		return id;
	}
	const auto threadID = theirVulkanFramework.GetMainThread();
	auto& allocSub = myAllocationSubmissions[int(id)];

	//const auto cmdPool = myCommandPools[threadID];
	while (myCommandBufferQueue[threadID].empty())
	{}
	auto cmdBuffer = myCommandBufferQueue[threadID].front();
	myCommandBufferQueue[threadID].pop();
	allocSub.Start(threadID, theirVulkanFramework.GetDevice(), cmdBuffer);

	return id;
}

AllocationSubmissionID
AllocationSubmitter::StartAllocSubmission(
	neat::ThreadID threadID)
{
	if (myCommandPools.find(threadID) == myCommandPools.cend())
	{
		RegisterThread(threadID);
		LOG("start called from unregistered thread!");
	}
	auto id = myAllocSubIDs.FetchFreeID();
	if (BAD_ID(id))
	{
		return id;
	}
	auto& allocSub = myAllocationSubmissions[int(id)];

	while (myCommandBufferQueue[threadID].empty())
	{
	}
	const auto cmdBuffer = myCommandBufferQueue[threadID].front();
	myCommandBufferQueue[threadID].pop();
	allocSub.Start(threadID, theirVulkanFramework.GetDevice(), cmdBuffer);

	return id;
}

void
AllocationSubmitter::QueueAllocSubmission(
	AllocationSubmissionID&& allocationSubmissionID)
{
	myAllocationSubmissions[int(allocationSubmissionID)].End();
	myQueuedSubmissions.push(std::move(allocationSubmissionID));
}

AllocationSubmitter::AllocationSubmitter(
	VulkanFramework& vulkanFramework, 
	QueueFamilyIndex transferFamilyIndex)
	: theirVulkanFramework(vulkanFramework)
	, myTransferFamily(transferFamilyIndex)
	, myAllocSubIDs(MaxNumAllocationSubmissions)
	, myAllocationSubmissions({})
{
	// COMMAND POOL
	VkCommandPoolCreateInfo cmdPoolInfo{};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = myTransferFamily;

	auto& cmdPool = myCommandPools[theirVulkanFramework.GetMainThread()];
	auto result = vkCreateCommandPool(theirVulkanFramework.GetDevice(), &cmdPoolInfo, nullptr, &cmdPool);
	assert(!result && "failed creating command pool");
}

AllocationSubmitter::~AllocationSubmitter()
{
	for (auto& [threadID, pool] : myCommandPools)
	{
		vkDestroyCommandPool(theirVulkanFramework.GetDevice(), pool, nullptr);
	}
}

void AllocationSubmitter::RegisterThread(neat::ThreadID threadID)
{
	// COMMAND POOL
	VkCommandPoolCreateInfo cmdPoolInfo{};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = myTransferFamily;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandPool pool;
	auto result = vkCreateCommandPool(theirVulkanFramework.GetDevice(), &cmdPoolInfo, nullptr, &pool);
	if (result)
	{
		assert(!result && "failed allocating thread allocation command buffers pool");
		return;
	}
	myCommandPools.insert({threadID, pool});

	VkCommandBufferAllocateInfo cmdBufferAlloc{};
	cmdBufferAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAlloc.commandBufferCount = MaxNumAllocationSubmissions;
	cmdBufferAlloc.commandPool = pool;
	cmdBufferAlloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

	std::array<VkCommandBuffer, MaxNumAllocationSubmissions> cmdBuffers = {};
	result = vkAllocateCommandBuffers(theirVulkanFramework.GetDevice(), &cmdBufferAlloc, cmdBuffers.data());
	if (result)
	{
		assert(!result && "failed allocating thread allocation command buffers");
		return;
	}
	myCommandBuffers.insert({threadID, cmdBuffers});
	
	for (size_t index = 0; index < cmdBuffers.size(); index++)
	{
		myCommandBufferQueue[threadID].push(cmdBuffers[index]);
		DebugSetObjectName(
			std::string("allocation submission cmd buffer, thread : ").append(std::to_string(int(threadID))).append(" nr : ").append(std::to_string(index)).c_str(),
			cmdBuffers[index],
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			theirVulkanFramework.GetDevice());
	}
}

bool
AllocatorBase::IsExclusive(
	const QueueFamilyIndex* firstOwner, 
	uint32_t				numOwners)
{
	bool exclusive = numOwners == 1 || !firstOwner;

	for (uint32_t outer = 0; outer < numOwners; ++outer)
	{
		for (uint32_t inner = 0; inner < numOwners; ++inner)
		{
			if (inner == outer)
				continue;
			if (firstOwner[outer] == firstOwner[inner])
			{
				exclusive = true;
			}
		}
	}

	return exclusive;
}

std::tuple<VkMemoryRequirements, MemTypeIndex>
AllocatorBase::GetMemReq(
	VkBuffer				buffer, 
	VkMemoryPropertyFlags	memPropFlags)
{
	VkMemoryRequirements memReq;
	vkGetBufferMemoryRequirements(theirVulkanFramework.GetDevice(), buffer, &memReq);

	int chosenIndex = UINT_MAX;
	for (auto& index : myMemoryTypes[memPropFlags])
	{
		if ((1 << index) & memReq.memoryTypeBits)
		{
			chosenIndex = index;
			break;
		}
	}

	return {memReq, chosenIndex};
}

std::tuple<VkMemoryRequirements, MemTypeIndex>
AllocatorBase::GetMemReq(
	VkImage					image, 
	VkMemoryPropertyFlags	memPropFlags)
{
	VkMemoryRequirements memReq;
	vkGetImageMemoryRequirements(theirVulkanFramework.GetDevice(), image, &memReq);

	MemTypeIndex chosenIndex = UINT_MAX;
	for (auto& index : myMemoryTypes[memPropFlags])
	{
		if ((1 << index) & memReq.memoryTypeBits)
		{
			chosenIndex = index;
			break;
		}
	}

	return {memReq, chosenIndex};
}

std::tuple<VkResult, BufferXMemory>
AllocatorBase::CreateStagingBuffer(
	const void*				data,
	size_t					size,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners)
{
	VkBuffer buffer{};
	VkDeviceMemory memory{};

	// BUFFER CREATION
	VkBufferCreateInfo bufferInfo;
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.flags = NULL;

	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	if (IsExclusive(firstOwner, numOwners))
	{
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.pQueueFamilyIndices = nullptr;
		bufferInfo.queueFamilyIndexCount = 0;
	}
	else
	{
		bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferInfo.pQueueFamilyIndices = firstOwner;
		bufferInfo.queueFamilyIndexCount = numOwners;
	}

	bufferInfo.size = size;

	auto resultBuffer = vkCreateBuffer(theirVulkanFramework.GetDevice(), &bufferInfo, nullptr, &buffer);

	if (resultBuffer)
	{
		LOG("failed to create buffer");
		return {resultBuffer, {buffer, memory}};
	}

	DebugSetObjectName("Staging Buffer", buffer, VK_OBJECT_TYPE_BUFFER, theirVulkanFramework.GetDevice());

	// MEM ALLOC
	auto [memReq, memTypeIndex] = GetMemReq(buffer, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	if (memTypeIndex == UINT_MAX)
	{
		LOG("no appropriate memory type found");
		return {VK_ERROR_FEATURE_NOT_PRESENT, {buffer, memory}};
	}

	VkMemoryAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.allocationSize = memReq.size;
	allocInfo.memoryTypeIndex = memTypeIndex;

	auto resultAlloc = vkAllocateMemory(theirVulkanFramework.GetDevice(), &allocInfo, nullptr, &memory);
	if (resultAlloc)
	{
		LOG("failed allocating memory");
		return {resultAlloc, {buffer, memory}};
	}

	// MEM BIND
	auto resultBind = vkBindBufferMemory(theirVulkanFramework.GetDevice(), buffer, memory, 0);
	if (resultBind)
	{
		LOG("failed to bind memory");
		return {resultBind, {buffer, memory}};
	}

	// MEM MAP
	void* mappedData;
	vkMapMemory(theirVulkanFramework.GetDevice(), memory, 0, size, NULL, &mappedData);
	memcpy(mappedData, data, size);
	vkUnmapMemory(theirVulkanFramework.GetDevice(), memory);

	return {VK_SUCCESS, {buffer, memory}};
}

void
AllocationSubmission::Start(
	neat::ThreadID	threadID,
	VkDevice		device,
	VkCommandBuffer	cmdBuffer)
{
	assert(myStatus == Status::Fresh);
	myThreadID = threadID;
	myDevice = device;
	myCommandBuffer = cmdBuffer;
	
	/*VkCommandBufferAllocateInfo cmdBufferAlloc{};
	cmdBufferAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufferAlloc.commandBufferCount = 1;
	cmdBufferAlloc.commandPool = myCommandPool;
	cmdBufferAlloc.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;

	auto result = vkAllocateCommandBuffers(myDevice, &cmdBufferAlloc, &myCommandBuffer);
	DebugSetObjectName(
		std::string("allocation submission cmd buffer, thread : ").append(std::to_string(int(myThreadID))).c_str(),
		myCommandBuffer,
		VK_OBJECT_TYPE_COMMAND_BUFFER,
		myDevice);*/

	VkCommandBufferInheritanceInfo inheritInfo = {};
	inheritInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pInheritanceInfo = &inheritInfo;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	vkBeginCommandBuffer(myCommandBuffer, &beginInfo);

	VkEventCreateInfo eventInfo{};
	eventInfo.sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;

	myExecutedEvent = std::make_shared<VkEvent>();
	auto result = vkCreateEvent(myDevice, &eventInfo, nullptr, &*myExecutedEvent);
	vkResetEvent(myDevice, *myExecutedEvent);

	myStatus = Status::Recording;
}

void
AllocationSubmission::End()
{
	assert(myStatus == Status::Recording);
	vkEndCommandBuffer(myCommandBuffer);
	myStatus = Status::PendingSubmit;
}
void
AllocationSubmission::AddResourceBuffer(
	VkBuffer		buffer,
	VkDeviceMemory	memory)
{
	assert(myStatus == Status::Recording);
	myBufferXMemorys.emplace_back(buffer, memory);
}

VkCommandBuffer
AllocationSubmission::Record() const
{
	assert(myStatus == Status::Recording && "allocation submission not in recording mode!");
	//assert(myCommandBuffer != nullptr && myExecutedEvent != nullptr && "Start() not called on allocation submission");
	return myCommandBuffer;
}

std::tuple<VkCommandBuffer, VkEvent>
AllocationSubmission::Submit(VkFence fence)
{
	assert(myStatus == Status::PendingSubmit);
	//assert(myCommandBuffer != nullptr && myExecutedEvent != nullptr && "Start() not called on allocation submission");
	myStatus = Status::PendingRelease;
	myExecutedFence = fence;
	return {myCommandBuffer, *myExecutedEvent};
}

std::tuple<bool, VkCommandBuffer>
AllocationSubmission::Release()
{
	assert(myCommandBuffer != nullptr && myExecutedEvent != nullptr && "Start() not called on allocation submission");
	assert(myExecutedFence != nullptr && "Submit() not called on allocation submission");
	assert(myStatus == Status::PendingRelease);

	if (vkGetEventStatus(myDevice, *myExecutedEvent) != VK_EVENT_SET)
	{
		return { false, nullptr };
	}

	while (vkGetFenceStatus(myDevice, myExecutedFence))
	{
	}

	for (const auto& stagingBuffer : myBufferXMemorys)
	{
		vkDestroyBuffer(myDevice, stagingBuffer.buffer, nullptr);
		vkFreeMemory(myDevice, stagingBuffer.memory, nullptr);
	}
	myBufferXMemorys.resize(myBufferXMemorys.size(), BufferXMemory{ nullptr, nullptr });
	myBufferXMemorys.clear();

	myExecutedFence = nullptr;
	vkDestroyEvent(myDevice, *myExecutedEvent, nullptr);
	*myExecutedEvent = nullptr;
	myExecutedEvent = nullptr;

	//myCommandPool = nullptr;
	myStatus = Status::Fresh;
	myDevice = nullptr;
	VkCommandBuffer cmdBuffer = myCommandBuffer;
	myCommandBuffer = nullptr;
	return { true, cmdBuffer };
}

std::shared_ptr<VkEvent>
AllocationSubmission::GetExecutedEvent() const
{
	assert(myStatus == Status::Recording);
	assert(myCommandBuffer != nullptr && myExecutedEvent != nullptr && "Start() not called on allocation submission");
	return myExecutedEvent;
}

_nodiscard neat::ThreadID AllocationSubmission::GetOwningThread() const
{
	return myThreadID;
}
