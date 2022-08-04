#include "pch.h"
#include "MeshHandler.h"

#include <vector>

#include "LoadMesh.h"
#include "Reflex/VK/Memory/BufferAllocator.h"
#include "Reflex/VK/Image/ImageHandler.h"

#include "HelperFuncs.h"
#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Memory/ImageAllocator.h"

MeshHandler::MeshHandler(
	VulkanFramework& vulkanFramework,
	BufferAllocator& bufferAllocator,
	ImageHandler& textureSetHandler,
	const QueueFamilyIndex* firstOwner,
	uint32_t				numOwners)
	: HandlerBase(vulkanFramework)
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
	poolSize.descriptorCount = MaxNumMeshesLoaded * 2 * NumSwapchainImages; // Index + Vertex

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.flags = NULL;

	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = NumSwapchainImages;

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

	for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
	{
		failure = vkAllocateDescriptorSets(theirVulkanFramework.GetDevice(), &allocInfo, &myMeshDataSets[swapchainIndex]);
		assert(!failure && "failed allocating mesh data set");
	}

	// FILL DEFAULT MESH DATA
	Vertex3D vertex{};
	uint32_t index = 0;

	AllocationSubmission allocSub = theirImageHandler.GetImageAllocator().Start();
	VkBuffer vBuffer, iBuffer;
	std::tie(failure, vBuffer) = theirBufferAllocator.RequestVertexBuffer(allocSub, { vertex }, myOwners);
	assert(!failure && "failed allocating default vertex buffer");
	std::tie(failure, iBuffer) = theirBufferAllocator.RequestIndexBuffer(allocSub, { index }, myOwners);
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

		VkDescriptorBufferInfo vBuffInfo{};
		vBuffInfo.buffer = vBuffer;
		vBuffInfo.offset = 0;
		vBuffInfo.range = VK_WHOLE_SIZE;
		vWrite.pBufferInfo = &vBuffInfo;

		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			vWrite.dstSet = myMeshDataSets[swapchainIndex];
			vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &vWrite, 0, nullptr);
		}

		VkWriteDescriptorSet iWrite{};
		iWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		iWrite.pNext = nullptr;

		iWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		iWrite.descriptorCount = 1;
		iWrite.dstArrayElement = i;
		iWrite.dstBinding = 1;

		VkDescriptorBufferInfo iBuffInfo{};
		iBuffInfo.buffer = iBuffer;
		iBuffInfo.offset = 0;
		iBuffInfo.range = VK_WHOLE_SIZE;
		iWrite.pBufferInfo = &iBuffInfo;

		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			iWrite.dstSet = myMeshDataSets[swapchainIndex];
			vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &iWrite, 0, nullptr);
		}
	}

	{
		std::vector<PixelValue> albedoPixels;
		albedoPixels.resize(64 * 64);
		for (size_t pixIndex = 0; pixIndex < albedoPixels.size(); ++pixIndex)
		{
			uint32_t y = pixIndex / 64;
			uint32_t x = pixIndex % 64;
			uint32_t g = (y + x) % 2;
			if (g == 0)
			{
				albedoPixels[pixIndex] = { 0, 0, 0, 255 };
			}
			else if (g == 1)
			{
				albedoPixels[pixIndex] = { 255, 255, 255, 255 };
			}
		}
		std::vector<uint8_t> albedoData;
		albedoData.resize(64 * 64 * 4);
		memcpy(albedoData.data(), albedoPixels.data(), albedoData.size());
		myMissingAlbedoID = theirImageHandler.AddImage2D(allocSub, std::move(albedoData), { 64, 64 });
	}
	{
		std::vector<PixelValue> materialPixels;
		materialPixels.resize(2 * 2, { 255, 0, 0, 0 });
		std::vector<uint8_t> materialData;
		materialData.resize(2 * 2 * 4);
		memcpy(materialData.data(), materialPixels.data(), materialData.size());
		myMissingMaterialID = theirImageHandler.AddImage2D(allocSub, std::move(materialData), { 2, 2 });
	}
	{
		std::vector<PixelValue> normalPixels;
		normalPixels.resize(2 * 2, { 127, 127, 255, 0 });
		std::vector<uint8_t> normalData;
		normalData.resize(2 * 2 * 4);
		memcpy(normalData.data(), normalPixels.data(), normalData.size());
		myMissingNormalID = theirImageHandler.AddImage2D(allocSub, std::move(normalData), { 2, 2 }, VK_FORMAT_R8G8B8A8_UNORM);
	}
	
	theirImageHandler.GetImageAllocator().Queue(std::move(allocSub));
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
	class AllocationSubmission& allocSub,
	const char*					path, 
	std::vector<ImageID>&&		imageIDs)
{
	MeshID meshID = myMeshIDKeeper.FetchFreeID();
	if (BAD_ID(meshID))
	{
		LOG("no more free mesh slots");
		return MeshID(INVALID_ID);
	}
	myMeshes[uint32_t(meshID)] = {};
	
	std::vector<Vec4f> imgIDs;
	if (imageIDs.empty())
	{
		// IMAGE ALLOC
		rapidjson::Document doc = OpenJsonDoc(std::filesystem::path(path).replace_extension("mx").string().c_str());
		imgIDs = LoadImagesFromDoc(doc, allocSub);
	}
	else
	{
		imageIDs.resize(4);
		imgIDs.resize(1);
		imgIDs[0].x = (BAD_ID(imageIDs[0]) || uint32_t(imageIDs[0]) == 0) ? float(myMissingAlbedoID) : float(imageIDs[0]);
		imgIDs[0].y = (BAD_ID(imageIDs[1]) || uint32_t(imageIDs[1]) == 0) ? float(myMissingMaterialID) : float(imageIDs[1]);
		imgIDs[0].z = (BAD_ID(imageIDs[2]) || uint32_t(imageIDs[2]) == 0) ? float(myMissingNormalID) : float(imageIDs[2]);
		imgIDs[0].w = (BAD_ID(imageIDs[3]) || uint32_t(imageIDs[3]) == 0) ? float(myMissingAlbedoID) : float(imageIDs[3]);
	}


	// BUFFER ALLOC
	RawMesh raw = LoadRawMesh(path, imgIDs);

	/*for (auto& v : raw.vertices)
	{
		v.texIDs.x = BAD_ID(texIDs.x) ? float(myMissingAlbedoID) : texIDs.x;
		v.texIDs.y = BAD_ID(texIDs.y) ? float(myMissingMaterialID) : texIDs.y;
		v.texIDs.z = BAD_ID(texIDs.z) ? float(myMissingNormalID) : texIDs.z;
		v.texIDs.w = BAD_ID(texIDs.w) ? 0 : texIDs.w;
	}*/

	auto [resultV, vBuffer] = theirBufferAllocator.RequestVertexBuffer(
		allocSub,
		raw.vertices,
		myOwners
	);
	auto [resultI, iBuffer] = theirBufferAllocator.RequestIndexBuffer(
		allocSub,
		raw.indices,
		myOwners
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
	WriteMeshDescriptorData(meshID, allocSub);

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
	int					swapchainIndex,
	VkCommandBuffer		cmdBuffer,
	VkPipelineLayout	layout,
	uint32_t			setIndex, VkPipelineBindPoint bindPoint)
{
	vkCmdBindDescriptorSets(cmdBuffer,
		bindPoint,
		layout,
		setIndex,
		1,
		&myMeshDataSets[swapchainIndex],
		0,
		nullptr);
}

std::vector<Vec4f>
MeshHandler::LoadImagesFromDoc(
	const rapidjson::Document& doc, AllocationSubmission& allocSub) const
{
	std::vector<Vec4f> imgIDs;

	if (doc.HasMember("Albedo"))
	{
		if (doc["Albedo"].IsArray())
		{
			uint32_t meshIndex = 0;
			for (auto& member : doc["Albedo"].GetArray())
			{
				if (member.IsInt())
				{
					meshIndex = member.GetInt();
					if (meshIndex + 1 > imgIDs.size())
					{
						imgIDs.resize(meshIndex + 1, {float(myMissingAlbedoID), float(myMissingMaterialID), float(myMissingNormalID), 0});
					}
				}
				else if (member.IsString())
				{
					const std::string& path = member.GetString();
					const ImageID imgID = theirImageHandler.AddImage2D(allocSub, path.c_str());
					imgIDs[meshIndex].x = BAD_ID(imgID) ? float(myMissingAlbedoID) : float(imgID);
				}
			}
		}
	}
	if (doc.HasMember("Material"))
	{
		if (doc["Material"].IsArray())
		{
			uint32_t meshIndex = 0;
			for (auto& member : doc["Material"].GetArray())
			{
				if (member.IsInt())
				{
					meshIndex = member.GetInt();
					if (meshIndex + 1 > imgIDs.size())
					{
						imgIDs.resize(meshIndex + 1, { float(myMissingAlbedoID), float(myMissingMaterialID), float(myMissingNormalID), 0 });
					}
				}
				else if (member.IsString())
				{
					const std::string& path = member.GetString();
					const ImageID imgID = theirImageHandler.AddImage2D(allocSub, path.c_str());
					imgIDs[meshIndex].y = BAD_ID(imgID) ? float(myMissingMaterialID) : float(imgID);
				}
			}
		}
	}
	if (doc.HasMember("Normal"))
	{
		if (doc["Normal"].IsArray())
		{
			uint32_t meshIndex = 0;
			for (auto& member : doc["Normal"].GetArray())
			{
				if (member.IsInt())
				{
					meshIndex = member.GetInt();
					if (meshIndex + 1 > imgIDs.size())
					{
						imgIDs.resize(meshIndex + 1, { float(myMissingAlbedoID), float(myMissingMaterialID), float(myMissingNormalID), 0 });
					}
				}
				else if (member.IsString())
				{
					const std::string& path = member.GetString();
					const ImageID imgID = theirImageHandler.AddImage2D(allocSub, path.c_str());
					imgIDs[meshIndex].z = BAD_ID(imgID) ? float(myMissingNormalID) : float(imgID);
				}
			}
		}
	}

	return imgIDs;
}

void
MeshHandler::WriteMeshDescriptorData(
	MeshID meshID, 
	AllocationSubmission& allocSub)
{
	{
		auto [vBuffer, iBuffer] = std::tuple{ myMeshes[int(meshID)].vertexBuffer,myMeshes[int(meshID)].indexBuffer };
		myMeshes[uint32_t(meshID)].vertexInfo.buffer = vBuffer;
		myMeshes[uint32_t(meshID)].vertexInfo.range = VK_WHOLE_SIZE;
		myMeshes[uint32_t(meshID)].indexInfo.buffer = iBuffer;
		myMeshes[uint32_t(meshID)].indexInfo.range = VK_WHOLE_SIZE;

		auto executedEvent = allocSub.GetExecutedEvent();
		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write.descriptorCount = 1;
			write.dstArrayElement = uint32_t(meshID);
			write.dstBinding = 0;
			write.pBufferInfo = &myMeshes[uint32_t(meshID)].vertexInfo;
			write.dstSet = myMeshDataSets[swapchainIndex];

			myQueuedDescriptorWrites[swapchainIndex].push({executedEvent, write});
		}
		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write.descriptorCount = 1;
			write.dstArrayElement = uint32_t(meshID);
			write.dstBinding = 1;
			write.pBufferInfo = &myMeshes[uint32_t(meshID)].indexInfo;
			write.dstSet = myMeshDataSets[swapchainIndex];

			myQueuedDescriptorWrites[swapchainIndex].push({executedEvent, write});
		}
	}
}
