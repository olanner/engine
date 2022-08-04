
#pragma once
#include "Mesh.h"
#include "Reflex/VK/Misc/HandlerBase.h"

class MeshHandler : public HandlerBase
{
public:
												MeshHandler(
													class VulkanFramework&	vulkanFramework,
													class BufferAllocator&	bufferAllocator,
													class ImageHandler&		textureSetHandler,
													const QueueFamilyIndex* firstOwner,
													uint32_t				numOwners);
												~MeshHandler();

	VkDescriptorSetLayout						GetImageArraySetLayout() const;

	MeshID										AddMesh(
													class AllocationSubmission& allocSub,
													const char*					path,
													std::vector<ImageID>&&		imageIDs = {});

	VkDescriptorSetLayout						GetMeshDataLayout();
	void										BindMeshData(
													int					swapchainIndex,
													VkCommandBuffer		cmdBuffer,
													VkPipelineLayout	layout,
													uint32_t			setIndex, 
													VkPipelineBindPoint bindPoint);

	Mesh										operator[](MeshID id) const;

private:
	std::vector<Vec4f>							LoadImagesFromDoc(
													const rapidjson::Document& doc, 
													AllocationSubmission& allocSub) const;
	void										WriteMeshDescriptorData(
													MeshID meshID, 
													AllocationSubmission& allocSub);

	BufferAllocator&							theirBufferAllocator;
	ImageHandler&								theirImageHandler;

	std::vector<QueueFamilyIndex>				myOwners;

	std::array<Mesh, MaxNumMeshesLoaded>		myMeshes = {};
	IDKeeper<MeshID>							myMeshIDKeeper;

	VkDescriptorPool							myDescriptorPool = nullptr;
	VkDescriptorSetLayout						myMeshDataLayout = nullptr;
	std::array<VkDescriptorSet, NumSwapchainImages>
												myMeshDataSets = {};

	ImageID										myMissingAlbedoID;
	ImageID										myMissingMaterialID;
	ImageID										myMissingNormalID;

};
