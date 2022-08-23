
#pragma once
#include "RFVK/Geometry/Vertex3D.h"

struct RawSubMeshDesc
{
	uint32_t firstVertexIndex = 0;
	uint32_t numVertices = 0;
	uint32_t firstIndexIndex = 0;
	uint32_t numIndices = 0;
};

struct RawMesh
{
	std::vector<RawSubMeshDesc> subMeshDescs;
	std::vector<Vertex3D>		vertices;
	std::vector<uint32_t>		indices;
};

RawMesh LoadRawMesh(
			const char*						filepath,
			neat::static_vector<Vec4f, 64>	imgIDs);