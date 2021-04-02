
#pragma once

struct Mesh
{
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	uint32_t numVertices;
	uint32_t numIndices;
};

static_assert(sizeof(Mesh) < 64);

void RecordMesh(
		VkCommandBuffer& cmdBuffer,
		const Mesh& mesh,
		uint32_t			firstInstance,
		uint32_t			numInstances);