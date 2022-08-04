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
	bool									Release();
	_nodiscard std::shared_ptr<VkEvent>		GetExecutedEvent() const;

private:
	void									Start(
												VkDevice		device,
												VkCommandPool	cmdPool);
	void									End();

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
	_nodiscard AllocationSubmission		StartAllocSubmission() const;
	_nodiscard AllocationSubmission		StartAllocSubmission(neat::ThreadID threadID);
	void								QueueAllocSubmission(AllocationSubmission&& allocationSubmission);

	std::optional<AllocationSubmission>	AcquireNextAllocSubmission();
	void								QueueRelease(AllocationSubmission&& allocationSubmission);
	void								TryReleasing();

private:
	VulkanFramework&					theirVulkanFramework;
	QueueFamilyIndex					myTransferFamily = 0;
	
	VkCommandPool						myCommandPool = nullptr;
	conc_map<neat::ThreadID, VkCommandPool>
										myThreadCommandPools;
	conc_map<neat::ThreadID, AllocationSubmission>
										myTransferSubmissions;
	conc_queue<AllocationSubmission>	myQueuedSubmissions;
	conc_queue<AllocationSubmission>	myQueuedReleases;
	
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
											uint32_t				numOwners,
											bool					forImmediateUse = false);

	VulkanFramework&					theirVulkanFramework;
	AllocationSubmitter&				theirAllocationSubmitter;
	ImmediateTransferrer&				theirImmediateTransferrer;
	
	QueueFamilyIndex					myTransferFamily;
	VkPhysicalDeviceMemoryProperties	myPhysicalDeviceMemProperties{};
	std::unordered_map<VkMemoryPropertyFlags, std::vector<MemTypeIndex>>
										myMemoryTypes;

};