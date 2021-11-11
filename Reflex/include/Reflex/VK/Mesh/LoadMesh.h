
#pragma once
#include "Reflex/VK/Geometry/Vertex3D.h"

struct RawSubMeshDesc
{
	uint32_t firstVertexIndex;
	uint32_t numVertices;
	uint32_t firstIndexIndex;
	uint32_t numIndices;
};

struct RawMesh
{
	std::vector<RawSubMeshDesc> subMeshDescs;
	std::vector<Vertex3D> vertices;
	std::vector<uint32_t> indices;
};

RawMesh LoadRawMesh(
			const char* filepath,
			std::vector<Vec4f> imgIDs);