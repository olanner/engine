
#pragma once
#include "Mesh.h"

class MeshHandler
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
													const char*				path, 
													std::vector<ImageID>&&	imageIDs = {});

	Mesh										operator[](MeshID id) const;

	VkDescriptorSetLayout						GetMeshDataLayout();
	void										BindMeshData(
													VkCommandBuffer		cmdBuffer,
													VkPipelineLayout	layout,
													uint32_t			setIndex,
													VkPipelineBindPoint bindPoint);

private:
	std::vector<Vec4f>							LoadImagesFromDoc(
													const rapidjson::Document& doc) const;
	void										WriteMeshDescriptorData(MeshID meshID);

	VulkanFramework&							theirVulkanFramework;
	BufferAllocator&							theirBufferAllocator;
	ImageHandler&								theirImageHandler;

	neat::static_vector<QueueFamilyIndex, 16>	myOwners;

	std::array<Mesh, MaxNumMeshesLoaded>		myMeshes;
	IDKeeper<MeshID>							myMeshIDKeeper;

	VkDescriptorPool							myDescriptorPool;
	VkDescriptorSetLayout						myMeshDataLayout;
	VkDescriptorSet								myMeshDataSet;

	ImageID										myMissingAlbedoID;
	ImageID										myMissingMaterialID;
	ImageID										myMissingNormalID;

};
