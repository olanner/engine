#include "pch.h"
#include "LoadMesh.h"

#pragma comment(lib, "assimp-vc140-mt.lib")

RawMesh LoadRawMesh(const char* filepath)
{
	const aiScene* scene = aiImportFile(filepath,
										 aiProcess_CalcTangentSpace |
										 aiProcess_JoinIdenticalVertices |
										 aiProcess_Triangulate |
										 aiProcess_ConvertToLeftHanded);

	const aiMesh* mesh = scene->mMeshes[0];
	RawMesh rawMesh;
	rawMesh.vertices.resize(mesh->mNumVertices);
	rawMesh.indices.resize(mesh->mNumFaces * 3);

	for (uint32_t i = 0; i < mesh->mNumVertices; i++)
	{
		rawMesh.vertices[i].position.x = mesh->mVertices[i].x;
		rawMesh.vertices[i].position.y = mesh->mVertices[i].y;
		rawMesh.vertices[i].position.z = mesh->mVertices[i].z;
		rawMesh.vertices[i].position.w = 1.f;

		rawMesh.vertices[i].normal.x = mesh->mNormals[i].x;
		rawMesh.vertices[i].normal.y = mesh->mNormals[i].y;
		rawMesh.vertices[i].normal.z = mesh->mNormals[i].z;
		rawMesh.vertices[i].normal.w = 0.f;

		rawMesh.vertices[i].tangent.x = mesh->mTangents[i].x;
		rawMesh.vertices[i].tangent.y = mesh->mTangents[i].y;
		rawMesh.vertices[i].tangent.z = mesh->mTangents[i].z;
		rawMesh.vertices[i].tangent.w = 0.f;

		rawMesh.vertices[i].uv.x = mesh->mTextureCoords[0][i].x;
		rawMesh.vertices[i].uv.y = mesh->mTextureCoords[0][i].y;
	}

	int currentIndex = 0;
	for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			rawMesh.indices[currentIndex++] = mesh->mFaces[i].mIndices[j];
		}
	}

	return rawMesh;
}
