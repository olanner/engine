#include "pch.h"
#include "LoadMesh.h"

#pragma comment(lib, "assimp-vc140-mt.lib")

RawMesh
LoadRawMesh(
	const char* filepath,
	std::vector<Vec4f> imgIDs)
{
	const aiScene* scene = aiImportFile(filepath,
		aiProcess_CalcTangentSpace
		| aiProcess_JoinIdenticalVertices
		| aiProcess_Triangulate
		| aiProcess_ConvertToLeftHanded);
	if (scene->mNumMeshes > imgIDs.size())
	{
		imgIDs.resize(scene->mNumMeshes);
	}
	RawMesh rawMesh;

	{
		uint32_t totalNumVertices = 0;
		uint32_t totalNumIndices = 0;
		for (uint32_t aiMeshIndex = 0; aiMeshIndex < scene->mNumMeshes; ++aiMeshIndex)
		{
			totalNumVertices += scene->mMeshes[0]->mNumVertices;
			totalNumIndices += scene->mMeshes[0]->mNumFaces * 3;
		}
		rawMesh.vertices.resize(totalNumVertices);
		rawMesh.indices.resize(totalNumIndices);
	}

	Vec3f max = {};
	uint32_t totalVertexIndex = 0;
	for (uint32_t aiMeshIndex = 0; aiMeshIndex < scene->mNumMeshes; ++aiMeshIndex)
	{
		const aiMesh* mesh = scene->mMeshes[aiMeshIndex];
		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; vertexIndex++)
		{
			rawMesh.vertices[totalVertexIndex].position.x = mesh->mVertices[vertexIndex].x;
			rawMesh.vertices[totalVertexIndex].position.y = mesh->mVertices[vertexIndex].y;
			rawMesh.vertices[totalVertexIndex].position.z = mesh->mVertices[vertexIndex].z;
			rawMesh.vertices[totalVertexIndex].position.w = 1.f;
			max.x = std::max(max.x, std::abs(rawMesh.vertices[totalVertexIndex].position.x));
			max.y = std::max(max.y, std::abs(rawMesh.vertices[totalVertexIndex].position.y));
			max.z = std::max(max.z, std::abs(rawMesh.vertices[totalVertexIndex].position.z));
			//rawMesh.vertices[totalVertexIndex].position = glm::normalize(rawMesh.vertices[totalVertexIndex].position);

			rawMesh.vertices[totalVertexIndex].normal.x = mesh->mNormals[vertexIndex].x;
			rawMesh.vertices[totalVertexIndex].normal.y = mesh->mNormals[vertexIndex].y;
			rawMesh.vertices[totalVertexIndex].normal.z = mesh->mNormals[vertexIndex].z;
			rawMesh.vertices[totalVertexIndex].normal.w = 0.f;

			rawMesh.vertices[totalVertexIndex].tangent.x = mesh->mTangents[vertexIndex].x;
			rawMesh.vertices[totalVertexIndex].tangent.y = mesh->mTangents[vertexIndex].y;
			rawMesh.vertices[totalVertexIndex].tangent.z = mesh->mTangents[vertexIndex].z;
			rawMesh.vertices[totalVertexIndex].tangent.w = 0.f;

			rawMesh.vertices[totalVertexIndex].uv.x = mesh->mTextureCoords[0][vertexIndex].x;
			rawMesh.vertices[totalVertexIndex].uv.y = mesh->mTextureCoords[0][vertexIndex].y;

			rawMesh.vertices[totalVertexIndex].texIDs = imgIDs[aiMeshIndex];
			totalVertexIndex++;
		}
	}
	
	/*float div = std::max({ max.x, max.y, max.z });
	for (auto& vertex : rawMesh.vertices)
	{
		vertex.position.x /= div;
		vertex.position.y /= div;
		vertex.position.z /= div;
	}*/

	uint32_t totalIndexIndex = 0;
	uint32_t indexOffset = 0;
	for (uint32_t aiMeshIndex = 0; aiMeshIndex < scene->mNumMeshes; ++aiMeshIndex)
	{
		const aiMesh* mesh = scene->mMeshes[aiMeshIndex];
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex)
		{
			for (uint32_t i = 0; i < 3; ++i)
			{
				rawMesh.indices[totalIndexIndex++] = mesh->mFaces[faceIndex].mIndices[i] + indexOffset;
			}
		}
		indexOffset += mesh->mNumVertices;
	}



	return rawMesh;
}
