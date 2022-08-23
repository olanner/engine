#pragma once

struct StagingBuffer
{
	VkBuffer		buffer;
	VkDeviceMemory	memory;
};

class AllocationSubmission
{
	friend class AllocationSubmitter;
	friend class ::concurrency::concurrent_queue<AllocationSubmission>;
	
	AllocationSubmission(const AllocationSubmission& copyFrom) = default;
	AllocationSubmission& operator=(const AllocationSubmission & copyFrom) = default;
	
	enum class Status
	{
		Fresh,
		
		Recording,
		PendingSubmit,
		Submitted,
		PendingRelease,
	};
	
public:
											AllocationSubmission() = default;
											AllocationSubmission(AllocationSubmission&& moveFrom) noexcept;
	AllocationSubmission&					operator=(AllocationSubmission&& moveFrom) noexcept;
	
	void									AddStagingBuffer(StagingBuffer&& stagingBuffer);
	_nodiscard VkCommandBuffer				Record() const;
	std::tuple<VkCommandBuffer, VkEvent>	Submit(VkFence fence);
	std::tuple<bool, VkCommandBuffer>		Release();
	_nodiscard std::shared_ptr<VkEvent>		GetExecutedEvent() const;
	_nodiscard neat::ThreadID				GetOwningThread() const;

private:
	void									Start(
												neat::ThreadID	threadID,
												VkDevice		device,
												VkCommandPool	cmdPool);
	void									End();

	neat::ThreadID							myThreadID;
	Status									myStatus = Status::Fresh;
	VkDevice								myDevice = nullptr;
	VkCommandPool							myCommandPool = nullptr;
	VkCommandBuffer							myCommandBuffer = nullptr;
	VkFence									myExecutedFence = nullptr;
	std::shared_ptr<VkEvent>				myExecutedEvent;
	std::vector<StagingBuffer>				myStagingBuffers;
	
};

class AllocationSubmitter final
{
public:
	
										AllocationSubmitter(
											class VulkanFramework&	vulkanFramework,
											QueueFamilyIndex		transferFamilyIndex);
										~AllocationSubmitter();
	
	void								RegisterThread(neat::ThreadID threadID);
	_nodiscard AllocationSubmission		StartAllocSubmission();
	_nodiscard AllocationSubmission		StartAllocSubmission(neat::ThreadID threadID);
	void								QueueAllocSubmission(AllocationSubmission&& allocationSubmission);

	std::optional<AllocationSubmission>	AcquireNextAllocSubmission();
	void								QueueRelease(AllocationSubmission&& allocationSubmission);
	
	void								TryReleasing();
	void								FreeUsedCommandBuffers(
											neat::ThreadID	threadID,
											int				maxCount);


private:
	VulkanFramework&					theirVulkanFramework;
	QueueFamilyIndex					myTransferFamily = 0;
	
	conc_map<neat::ThreadID, VkCommandPool>
										myCommandPools;
	conc_map<neat::ThreadID, AllocationSubmission>
										myTransferSubmissions;
	conc_queue<AllocationSubmission>	myQueuedSubmissions;
	conc_queue<AllocationSubmission>	myQueuedReleases;
	conc_map<neat::ThreadID, conc_queue<VkCommandBuffer>>
										myQueuedCommandBufferDeallocs;
	
	
};

class AllocatorBase
{
	friend class VulkanImplementation;
public:
										AllocatorBase(
											class VulkanFramework&		vulkanFramework,
											AllocationSubmitter&		allocationSubmitter,
											class ImmediateTransferrer& immediateTransferrer,
											QueueFamilyIndex			transferFamilyIndex);
	virtual								~AllocatorBase();

	_nodiscard AllocationSubmission		Start() const;
	_nodiscard AllocationSubmission		Start(neat::ThreadID threadID) const;
	void								Queue(AllocationSubmission&& allocationSubmission) const;

	virtual void						DoCleanUp(int limit) = 0;

protected:
	bool								IsExclusive(
											const QueueFamilyIndex* firstOwner,
											uint32_t numOwners);
	std::tuple<VkMemoryRequirements, MemTypeIndex>
										GetMemReq(
											VkBuffer				buffer, 
											VkMemoryPropertyFlags	memPropFlags);
	std::tuple<VkMemoryRequirements, MemTypeIndex>
										GetMemReq(
											VkImage					image, 
											VkMemoryPropertyFlags	memPropFlags);

	[[nodiscard]] std::tuple<VkResult, StagingBuffer>
										CreateStagingBuffer(
											const void*				data,
											size_t					size,
											const QueueFamilyIndex*	firstOwner,
											uint32_t				numOwners);

	VulkanFramework&					theirVulkanFramework;
	AllocationSubmitter&				theirAllocationSubmitter;
	
	QueueFamilyIndex					myTransferFamily;
	VkPhysicalDeviceMemoryProperties	myPhysicalDeviceMemProperties{};
	std::unordered_map<VkMemoryPropertyFlags, std::vector<MemTypeIndex>>
										myMemoryTypes;

};