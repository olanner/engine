
#pragma once

struct MeshGeometry
{
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	uint32_t numVertices;
	uint32_t numIndices;
};

void RecordMesh(
		VkCommandBuffer& cmdBuffer,
		const MeshGeometry& mesh,
		uint32_t			firstInstance,
		uint32_t			numInstances);