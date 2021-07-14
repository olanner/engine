#include "pch.h"
#include "MeshHandler.h"
#include "LoadMesh.h"
#include "Reflex/VK/Memory/BufferAllocator.h"
#include "Reflex/VK/Image/ImageHandler.h"

#include "HelperFuncs.h"
#include "Reflex/VK/VulkanFramework.h"

MeshHandler::MeshHandler(
	VulkanFramework& vulkanFramework,
	BufferAllocator& bufferAllocator,
	ImageHandler& textureSetHandler,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners)
	: theirVulkanFramework(vulkanFramework)
	, theirBufferAllocator(bufferAllocator)
	, theirImageHandler(textureSetHandler)
	, myMeshIDKeeper(MaxNumMeshesLoaded)
{
	for (uint32_t i = 0; i < numOwners; ++i)
	{
		myOwners.emplace_back(firstOwner[i]);
	}

	VkDescriptorPoolSize poolSize;
	poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSize.descriptorCount = MaxNumImages2D * 2; // Index + Vertex

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = NULL;

	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	auto failure = vkCreateDescriptorPool(theirVulkanFramework.GetDevice(), &poolInfo, nullptr, &myDescriptorPool);
	assert(!failure && "failed creating desc pool");

	// MESH DATA LAYOUT
	VkDescriptorSetLayoutBinding vertexBinding{};
	vertexBinding.binding = 0;
	vertexBinding.descriptorCount = MaxNumMeshesLoaded;
	vertexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vertexBinding.stageFlags =
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;
	vertexBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding indexBinding{};
	indexBinding.binding = 1;
	indexBinding.descriptorCount = MaxNumMeshesLoaded;
	indexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	indexBinding.stageFlags =
		VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV |
		VK_SHADER_STAGE_ANY_HIT_BIT_NV |
		VK_SHADER_STAGE_RAYGEN_BIT_NV |
		VK_SHADER_STAGE_MISS_BIT_NV;
	indexBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.flags = NULL;

	VkDescriptorSetLayoutBinding bindings[]
	{
		vertexBinding,
		indexBinding
	};

	layoutInfo.bindingCount = ARRAYSIZE(bindings);
	layoutInfo.pBindings = bindings;

	failure = vkCreateDescriptorSetLayout(theirVulkanFramework.GetDevice(), &layoutInfo, nullptr, &myMeshDataLayout);
	assert(!failure && "failed creating mesh data layout");

	// MESH DATA SET
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;

	allocInfo.descriptorPool = myDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &myMeshDataLayout;

	failure = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &myMeshDataSet);
	assert(!failure && "failed allocating mesh data set");

	// FILL DEFAULT MESH DATA
	Vertex3D vertex{};
	uint32_t index = 0;

	VkBuffer vBuffer, iBuffer;
	std::tie(failure, vBuffer) = theirBufferAllocator.RequestVertexBuffer(&vertex, 1, myOwners.data(), myOwners.size());
	assert(!failure && "failed allocating default vertex buffer");
	std::tie(failure, iBuffer) = theirBufferAllocator.RequestIndexBuffer(&index, 1, myOwners.data(), myOwners.size());
	assert(!failure && "failed allocating default index buffer");
	for (uint32_t i = 0; i < MaxNumMeshesLoaded; ++i)
	{
		VkWriteDescriptorSet vWrite{};
		vWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vWrite.pNext = nullptr;

		vWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		vWrite.descriptorCount = 1;
		vWrite.dstArrayElement = i;
		vWrite.dstBinding = 0;
		vWrite.dstSet = myMeshDataSet;

		VkDescriptorBufferInfo vBuffInfo{};
		vBuffInfo.buffer = vBuffer;
		vBuffInfo.offset = 0;
		vBuffInfo.range = VK_WHOLE_SIZE;
		vWrite.pBufferInfo = &vBuffInfo;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &vWrite, 0, nullptr);

		VkWriteDescriptorSet iWrite{};
		iWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		iWrite.pNext = nullptr;

		iWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		iWrite.descriptorCount = 1;
		iWrite.dstArrayElement = i;
		iWrite.dstBinding = 1;
		iWrite.dstSet = myMeshDataSet;

		VkDescriptorBufferInfo iBuffInfo{};
		iBuffInfo.buffer = iBuffer;
		iBuffInfo.offset = 0;
		iBuffInfo.range = VK_WHOLE_SIZE;
		iWrite.pBufferInfo = &iBuffInfo;

		vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &iWrite, 0, nullptr);
	}
}

MeshHandler::~MeshHandler()
{
}

VkDescriptorSetLayout
MeshHandler::GetImageArraySetLayout() const
{
	return theirImageHandler.GetImageSetLayout();
}

MeshID
MeshHandler::AddMesh(
	const char* path,
	std::vector<ImageID>&& imageIDs)
{
	MeshID meshID = myMeshIDKeeper.FetchFreeID();
	if (BAD_ID(meshID))
	{
		LOG("no more free mesh slots");
		return MeshID(INVALID_ID);
	}
	Vec4f texIDs{};
	if (imageIDs.empty())
	{
		// IMAGE ALLOC
		rapidjson::Document doc = OpenJsonDoc(std::filesystem::path(path).replace_extension("mx").string().c_str());
		texIDs = LoadImagesFromDoc(doc);
	}
	else
	{
		imageIDs.resize(4);
		texIDs.x = BAD_ID(imageIDs[0]) ? 0 : float(imageIDs[0]);
		texIDs.y = BAD_ID(imageIDs[1]) ? 0 : float(imageIDs[1]);
		texIDs.z = BAD_ID(imageIDs[2]) ? 0 : float(imageIDs[2]);
		texIDs.w = BAD_ID(imageIDs[3]) ? 0 : float(imageIDs[3]);
	}


	// BUFFER ALLOC
	RawMesh raw = LoadRawMesh(path);

	for (auto& v : raw.vertices)
	{
		v.texIDs.x = BAD_ID(texIDs.x) ? 0 : texIDs.x;
		v.texIDs.y = BAD_ID(texIDs.y) ? 0 : texIDs.y;
		v.texIDs.z = BAD_ID(texIDs.z) ? 0 : texIDs.z;
		v.texIDs.w = BAD_ID(texIDs.w) ? 0 : texIDs.w;
	}

	auto [resultV, vBuffer] = theirBufferAllocator.RequestVertexBuffer(raw.vertices.data(),
		uint32_t(raw.vertices.size()),
		myOwners.data(),
		myOwners.size()
	);
	auto [resultI, iBuffer] = theirBufferAllocator.RequestIndexBuffer(raw.indices.data(),
		uint32_t(raw.indices.size()),
		myOwners.data(),
		myOwners.size()
	);
	if (resultV || resultI)
	{
		LOG("failed loading mesh");
		return MeshID(INVALID_ID);
	}

	myMeshes[int(meshID)].vertexBuffer = vBuffer;
	myMeshes[int(meshID)].indexBuffer = iBuffer;
	myMeshes[int(meshID)].numVertices = uint32_t(raw.vertices.size());
	myMeshes[int(meshID)].numIndices = uint32_t(raw.indices.size());

	// DESCRIPTOR WRITE
	WriteMeshDescriptorData(meshID);

	return meshID;
}

Mesh
MeshHandler::operator[](MeshID id) const
{
	return myMeshes[int(id)];
}

VkDescriptorSetLayout
MeshHandler::GetMeshDataLayout()
{
	return myMeshDataLayout;
}

void
MeshHandler::BindMeshData(
	VkCommandBuffer		cmdBuffer,
	VkPipelineLayout	layout,
	uint32_t			setIndex,
	VkPipelineBindPoint bindPoint)
{
	vkCmdBindDescriptorSets(cmdBuffer,
		bindPoint,
		layout,
		setIndex,
		1,
		&myMeshDataSet,
		0,
		nullptr);
}

Vec4f
MeshHandler::LoadImagesFromDoc(
	const rapidjson::Document& doc) const
{
	ImageID
		albedoID = ImageID(INVALID_ID),
		materialID = ImageID(INVALID_ID);

	if (doc.HasMember("Albedo"))
	{
		char* raw = (char*)doc["Albedo"].GetString();
		char texPath[128]{};
		strcpy_s(texPath, raw);
		albedoID = theirImageHandler.AddImage2D(texPath);
		assert(!BAD_ID(albedoID) && "failed creating image");
	}
	if (doc.HasMember("Material"))
	{
		char* raw = (char*)doc["Material"].GetString();
		char texPath[128]{};
		strcpy_s(texPath, raw);
		materialID = theirImageHandler.AddImage2D(texPath);
		assert(!BAD_ID(materialID) && "failed creating image");
	}

	return
	{
		float(albedoID),
		float(materialID),
		0,
		0
	};
}

void
MeshHandler::WriteMeshDescriptorData(MeshID meshID)
{
	auto [vBuffer, iBuffer] = std::tuple<VkBuffer, VkBuffer>{ myMeshes[int(meshID)].vertexBuffer,myMeshes[int(meshID)].indexBuffer };


	VkWriteDescriptorSet vWrite{};
	vWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	vWrite.pNext = nullptr;

	vWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	vWrite.descriptorCount = 1;
	vWrite.dstArrayElement = uint32_t(meshID);
	vWrite.dstBinding = 0;
	vWrite.dstSet = myMeshDataSet;

	VkDescriptorBufferInfo vBuffInfo{};
	vBuffInfo.buffer = vBuffer;
	vBuffInfo.offset = 0;
	vBuffInfo.range = VK_WHOLE_SIZE;
	vWrite.pBufferInfo = &vBuffInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &vWrite, 0, nullptr);

	VkWriteDescriptorSet iWrite{};
	iWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	iWrite.pNext = nullptr;

	iWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	iWrite.descriptorCount = 1;
	iWrite.dstArrayElement = uint32_t(meshID);
	iWrite.dstBinding = 1;
	iWrite.dstSet = myMeshDataSet;

	VkDescriptorBufferInfo iBuffInfo{};
	iBuffInfo.buffer = iBuffer;
	iBuffInfo.offset = 0;
	iBuffInfo.range = VK_WHOLE_SIZE;
	iWrite.pBufferInfo = &iBuffInfo;

	vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &iWrite, 0, nullptr);
}
