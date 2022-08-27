
#pragma once
#include "Mesh.h"
#include "RFVK/Misc/HandlerBase.h"

struct Mesh
{
	VkDescriptorBufferInfo			vertexInfo;
	VkDescriptorBufferInfo			indexInfo;
	MeshGeometry					geo;
	neat::static_vector<Vec4f, 64>	imageIDs;
};

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

	MeshID										AddMesh();
	void										RemoveMesh(MeshID meshID);
	void										UnloadMesh(MeshID meshID);
	void										LoadMesh(
													MeshID meshID,
													AllocationSubmissionID		allocSubID,
													const std::string&			path,
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
	neat::static_vector<Vec4f, 64>				LoadImagesFromDoc(
													const rapidjson::Document& doc, 
													AllocationSubmissionID allocSubID) const;
	void										WriteMeshDescriptorData(
													MeshID meshID, 
													AllocationSubmissionID allocSubID);

	BufferAllocator&							theirBufferAllocator;
	ImageHandler&								theirImageHandler;

	std::vector<QueueFamilyIndex>				myOwners;

	Mesh										myDefaultMesh = {};
	std::array<Mesh, MaxNumMeshesLoaded>		myMeshes = {};
	IDKeeper<MeshID>							myMeshIDKeeper;

	VkDescriptorPool							myDescriptorPool = nullptr;
	VkDescriptorSetLayout						myMeshDataLayout = nullptr;
	std::array<VkDescriptorSet, NumSwapchainImages>
												myMeshDataSets = {};

	std::array<ImageID, 4>						myMissingImageIDs;
	//ImageID										myMissingAlbedoID;
	//ImageID										myMissingMaterialID;
	//ImageID										myMissingNormalID;

};
