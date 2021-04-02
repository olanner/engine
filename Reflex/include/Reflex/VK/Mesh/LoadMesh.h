
#pragma once
#include "Reflex/VK/Geometry/Vertex3D.h"

struct RawMesh
{
	std::vector<Vertex3D> vertices;
	std::vector<uint32_t> indices;
};

RawMesh LoadRawMesh(const char* filepath);