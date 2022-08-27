#include "pch.h"
#include "MeshHandler.h"

#include <vector>

#include "LoadMesh.h"
#include "RFVK/Memory/BufferAllocator.h"
#include "RFVK/Image/ImageHandler.h"

#include "RFVK/VulkanFramework.h"
#include "RFVK/Memory/ImageAllocator.h"

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

	// LOAD DEFAULT IMAGES
	AllocationSubmissionID allocSubID = theirImageHandler.GetImageAllocator().Start();
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
		myMissingImageIDs[0] = theirImageHandler.AddImage2D();
		theirImageHandler.LoadImage2D(myMissingImageIDs[0], allocSubID, std::move(albedoData), { 64, 64 });
	}
	{
		std::vector<PixelValue> materialPixels;
		materialPixels.resize(2 * 2, { 255, 0, 0, 0 });
		std::vector<uint8_t> materialData;
		materialData.resize(2 * 2 * 4);
		memcpy(materialData.data(), materialPixels.data(), materialData.size());
		myMissingImageIDs[1] = theirImageHandler.AddImage2D();
		theirImageHandler.LoadImage2D(myMissingImageIDs[1], allocSubID, std::move(materialData), { 2, 2 });
	}
	{
		std::vector<PixelValue> normalPixels;
		normalPixels.resize(2 * 2, { 127, 127, 255, 0 });
		std::vector<uint8_t> normalData;
		normalData.resize(2 * 2 * 4);
		memcpy(normalData.data(), normalPixels.data(), normalData.size());
		myMissingImageIDs[2] = theirImageHandler.AddImage2D();
		theirImageHandler.LoadImage2D(myMissingImageIDs[2], allocSubID, std::move(normalData), { 2, 2 }, VK_FORMAT_R8G8B8A8_UNORM);
	}

	// FILL DEFAULT MESH DATA
	const auto rawMesh = LoadRawMesh("cube.dae", { {myMissingImageIDs[0], myMissingImageIDs[1], myMissingImageIDs[2], 0} });
	
	VkBuffer vBuffer, iBuffer;
	std::tie(failure, vBuffer) = theirBufferAllocator.RequestVertexBuffer(allocSubID, rawMesh.vertices, myOwners);
	assert(!failure && "failed allocating default vertex buffer");
	std::tie(failure, iBuffer) = theirBufferAllocator.RequestIndexBuffer(allocSubID, rawMesh.indices, myOwners);
	assert(!failure && "failed allocating default index buffer");
	
	Mesh defaultMesh = {};
	defaultMesh.geo.vertexBuffer = vBuffer;
	defaultMesh.geo.indexBuffer = iBuffer;
	defaultMesh.geo.numVertices = uint32_t(rawMesh.vertices.size());
	defaultMesh.geo.numIndices = uint32_t(rawMesh.indices.size());
	defaultMesh.imageIDs.emplace_back(Vec4f{myMissingImageIDs[0], myMissingImageIDs[1], myMissingImageIDs[2], 0});
	
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

		defaultMesh.vertexInfo = vBuffInfo;

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

		defaultMesh.indexInfo = iBuffInfo;

		for (int swapchainIndex = 0; swapchainIndex < NumSwapchainImages; ++swapchainIndex)
		{
			iWrite.dstSet = myMeshDataSets[swapchainIndex];
			vkUpdateDescriptorSets(theirVulkanFramework.GetDevice(), 1, &iWrite, 0, nullptr);
		}
	}

	myDefaultMesh = defaultMesh;
	myMeshes.fill(defaultMesh);
	
	theirImageHandler.GetImageAllocator().Queue(std::move(allocSubID));
}

MeshHandler::~MeshHandler()
{
}

VkDescriptorSetLayout
MeshHandler::GetImageArraySetLayout() const
{
	return theirImageHandler.GetImageSetLayout();
}

MeshID MeshHandler::AddMesh()
{
	MeshID meshID = myMeshIDKeeper.FetchFreeID();
	if (BAD_ID(meshID))
	{
		LOG("no more free mesh slots");
		return MeshID(INVALID_ID);
	}
	return meshID;
}

void
MeshHandler::RemoveMesh(
	MeshID meshID)
{
	UnloadMesh(meshID);
	myMeshIDKeeper.ReturnID(meshID);
}

void
MeshHandler::UnloadMesh(
	MeshID meshID)
{
	if (BAD_ID(meshID))
	{
		LOG("invalid id passed to UnloadMesh()");
		return;
	}
	auto& mesh = myMeshes[int(meshID)];
	if (mesh.geo.vertexBuffer == myDefaultMesh.geo.vertexBuffer)
	{
		LOG("mesh with id: ", int(meshID), ", already unloaded");
		return;
	}
	for (auto& ids : mesh.imageIDs)
	{
		for (auto& id : {ids.x, ids.y, ids.z, ids.w})
		{
			if (std::ranges::find(myMissingImageIDs, ImageID(id)) == myMissingImageIDs.end()
				&& int(id) != 0)
			{
				theirImageHandler.UnloadImage2D(ImageID(id));
			}
		}
	}
	mesh.imageIDs.clear();
	mesh = myDefaultMesh;
}

void
MeshHandler::LoadMesh(
	MeshID meshID,
	AllocationSubmissionID allocSubID,
	const std::string&			path,
	std::vector<ImageID>&&		imageIDs)
{
	if (BAD_ID(meshID))
	{
		LOG("invalid id passed to load mesh");
		return;
	}
	auto& mesh = myMeshes[int(meshID)];
	
	//neat::static_vector<Vec4f, 64> imgIDs;
	if (imageIDs.empty())
	{
		// IMAGE ALLOC
		rapidjson::Document doc = OpenJsonDoc(std::filesystem::path(path).replace_extension("mx").string().c_str());
		mesh.imageIDs = LoadImagesFromDoc(doc, allocSubID);
	}
	else
	{
		imageIDs.resize(4, ImageID(-1));
		mesh.imageIDs.resize(1);
		mesh.imageIDs[0].x = (BAD_ID(imageIDs[0]) || uint32_t(imageIDs[0]) == 0) ? float(myMissingImageIDs[0]) : float(imageIDs[0]);
		mesh.imageIDs[0].y = (BAD_ID(imageIDs[1]) || uint32_t(imageIDs[1]) == 0) ? float(myMissingImageIDs[1]) : float(imageIDs[1]);
		mesh.imageIDs[0].z = (BAD_ID(imageIDs[2]) || uint32_t(imageIDs[2]) == 0) ? float(myMissingImageIDs[2]) : float(imageIDs[2]);
		mesh.imageIDs[0].w = (BAD_ID(imageIDs[3]) || uint32_t(imageIDs[3]) == 0) ? float(myMissingImageIDs[3]) : float(imageIDs[3]);
	}


	// BUFFER ALLOC
	const auto rawMesh = LoadRawMesh(path.c_str(), mesh.imageIDs);

	/*for (auto& v : raw.vertices)
	{
		v.texIDs.x = BAD_ID(texIDs.x) ? float(myMissingAlbedoID) : texIDs.x;
		v.texIDs.y = BAD_ID(texIDs.y) ? float(myMissingMaterialID) : texIDs.y;
		v.texIDs.z = BAD_ID(texIDs.z) ? float(myMissingNormalID) : texIDs.z;
		v.texIDs.w = BAD_ID(texIDs.w) ? 0 : texIDs.w;
	}*/

	auto [resultV, vBuffer] = theirBufferAllocator.RequestVertexBuffer(
		allocSubID,
		rawMesh.vertices,
		myOwners
	);
	auto [resultI, iBuffer] = theirBufferAllocator.RequestIndexBuffer(
		allocSubID,
		rawMesh.indices,
		myOwners
	);
	if (resultV || resultI)
	{
		LOG("failed loading mesh");
		return;
	}

	mesh.geo.vertexBuffer = vBuffer;
	mesh.geo.indexBuffer = iBuffer;
	mesh.geo.numVertices = uint32_t(rawMesh.vertices.size());
	mesh.geo.numIndices = uint32_t(rawMesh.indices.size());

	VkBufferDeviceAddressInfo addressInfo = {};
	addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	addressInfo.buffer = vBuffer;
	mesh.geo.vertexAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);
	addressInfo.buffer = iBuffer;
	mesh.geo.indexAddress = vkGetBufferDeviceAddress(theirVulkanFramework.GetDevice(), &addressInfo);

	// DESCRIPTOR WRITE
	WriteMeshDescriptorData(meshID, allocSubID);
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
	uint32_t			setIndex, 
	VkPipelineBindPoint bindPoint)
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

neat::static_vector<Vec4f, 64>
MeshHandler::LoadImagesFromDoc(
	const rapidjson::Document& doc, 
	AllocationSubmissionID allocSubID) const
{
	neat::static_vector<Vec4f, 64> imgIDs;

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
						imgIDs.resize(meshIndex + 1, {float(myMissingImageIDs[0]), float(myMissingImageIDs[1]), float(myMissingImageIDs[2]), 0});
					}
				}
				else if (member.IsString())
				{
					const std::string path = member.GetString();
					const ImageID imgID = theirImageHandler.AddImage2D();
					theirImageHandler.LoadImage2D(imgID, allocSubID, path);
					imgIDs[meshIndex].x = BAD_ID(imgID) ? float(myMissingImageIDs[0]) : float(imgID);
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
						imgIDs.resize(meshIndex + 1, { float(myMissingImageIDs[0]), float(myMissingImageIDs[1]), float(myMissingImageIDs[2]), 0 });
					}
				}
				else if (member.IsString())
				{
					const std::string path = member.GetString();
					const ImageID imgID = theirImageHandler.AddImage2D();
					theirImageHandler.LoadImage2D(imgID, allocSubID, path);
					imgIDs[meshIndex].y = BAD_ID(imgID) ? float(myMissingImageIDs[1]) : float(imgID);
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
						imgIDs.resize(meshIndex + 1, { float(myMissingImageIDs[0]), float(myMissingImageIDs[1]), float(myMissingImageIDs[2]), 0 });
					}
				}
				else if (member.IsString())
				{
					const std::string path = member.GetString();
					const ImageID imgID = theirImageHandler.AddImage2D();
					theirImageHandler.LoadImage2D(imgID, allocSubID, path);
					imgIDs[meshIndex].z = BAD_ID(imgID) ? float(myMissingImageIDs[2]) : float(imgID);
				}
			}
		}
	}

	return imgIDs;
}

void
MeshHandler::WriteMeshDescriptorData(
	MeshID					meshID, 
	AllocationSubmissionID	allocSubID)
{
	{
		auto [vBuffer, iBuffer] = std::tuple{ myMeshes[int(meshID)].geo.vertexBuffer,myMeshes[int(meshID)].geo.indexBuffer };
		myMeshes[uint32_t(meshID)].vertexInfo.buffer = vBuffer;
		myMeshes[uint32_t(meshID)].vertexInfo.range = VK_WHOLE_SIZE;
		myMeshes[uint32_t(meshID)].indexInfo.buffer = iBuffer;
		myMeshes[uint32_t(meshID)].indexInfo.range = VK_WHOLE_SIZE;

		auto& allocSub = theirBufferAllocator.GetAllocationSubmission(allocSubID);
		const auto executedEvent = allocSub.GetExecutedEvent();
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
			QueueDescriptorUpdate(
				swapchainIndex, 
				executedEvent, 
				nullptr, 
				write);
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
			QueueDescriptorUpdate(
				swapchainIndex, 
				executedEvent, 
				nullptr, 
				write);
		}
	}
}
