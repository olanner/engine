#pragma once

struct BufferXMemory
{
	VkBuffer		buffer;
	VkDeviceMemory	memory;
};

constexpr int MaxNumAllocationSubmissions = 128;
class AllocationSubmission
{
	friend class AllocationSubmitter;
	friend class ::concurrency::concurrent_queue<AllocationSubmission>;
	
	//AllocationSubmission(const AllocationSubmission& copyFrom) = default;
	//AllocationSubmission& operator=(const AllocationSubmission & copyFrom) = default;
	
	enum class Status
	{
		Fresh,
		
		Recording,
		PendingSubmit,
		Submitted,
		PendingRelease,
	};
	
											AllocationSubmission() = default;
public:
	//										AllocationSubmission(AllocationSubmission&& moveFrom) noexcept;
	//AllocationSubmission&					operator=(AllocationSubmission&& moveFrom) noexcept;
	
	void									AddResourceBuffer(
												VkBuffer buffer,
												VkDeviceMemory memory);
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

	neat::ThreadID							myThreadID = neat::ThreadID(INVALID_ID);
	Status									myStatus = Status::Fresh;
	VkDevice								myDevice = nullptr;
	VkCommandPool							myCommandPool = nullptr;
	VkCommandBuffer							myCommandBuffer = nullptr;
	VkFence									myExecutedFence = nullptr;
	std::shared_ptr<VkEvent>				myExecutedEvent = nullptr;
	std::vector<BufferXMemory>				myBufferXMemorys;
};

class AllocationSubmitter final
{
public:
	
											AllocationSubmitter(
												class VulkanFramework&	vulkanFramework,
												QueueFamilyIndex		transferFamilyIndex);
											~AllocationSubmitter();
	
	void									RegisterThread(neat::ThreadID threadID);
	_nodiscard AllocationSubmissionID		StartAllocSubmission();
	_nodiscard AllocationSubmissionID		StartAllocSubmission(neat::ThreadID threadID);
	void									QueueAllocSubmission(AllocationSubmissionID&& allocationSubmissionID);

	std::optional<AllocationSubmissionID>	AcquireNextAllocSubmission();
	void									QueueRelease(AllocationSubmissionID&& allocationSubmissionID);
	
	void									TryReleasing();
	void									FreeUsedCommandBuffers(
												neat::ThreadID	threadID,
												int				maxCount);
	AllocationSubmission&					operator[](AllocationSubmissionID id);

private:
	VulkanFramework&					theirVulkanFramework;
	QueueFamilyIndex					myTransferFamily = 0;
	
	conc_map<neat::ThreadID, VkCommandPool>
										myCommandPools;
	conc_map<neat::ThreadID, AllocationSubmissionID>
										myTransferSubmissions;
	conc_queue<AllocationSubmissionID>	myQueuedSubmissions;
	conc_queue<AllocationSubmissionID>	myQueuedReleases;
	conc_map<neat::ThreadID, conc_queue<VkCommandBuffer>>
										myQueuedCommandBufferDeallocs;

	IDKeeper<AllocationSubmissionID>	myAllocSubIDs;
	std::array<AllocationSubmission, MaxNumAllocationSubmissions>
										myAllocationSubmissions;
	conc_map<neat::ThreadID,std::array<VkCommandBuffer, MaxNumAllocationSubmissions>>
										myCommandBuffers;
	
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

	_nodiscard AllocationSubmissionID		Start() const;
	_nodiscard AllocationSubmissionID		Start(neat::ThreadID threadID) const;
	void									Queue(AllocationSubmissionID&& allocationSubmissionID) const;
	AllocationSubmission&					GetAllocationSubmission(AllocationSubmissionID allocationSubmissionID) const;

	virtual void							DoCleanUp(int limit) = 0;

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

	[[nodiscard]] std::tuple<VkResult, BufferXMemory>
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