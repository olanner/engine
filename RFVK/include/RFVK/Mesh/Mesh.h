
#pragma once

struct MeshGeometry
{
	VkBuffer		vertexBuffer = nullptr;
	VkBuffer		indexBuffer = nullptr;
	VkDeviceAddress vertexAddress = 0;
	VkDeviceAddress indexAddress = 0;
	uint32_t		numVertices = 0;
	uint32_t		numIndices = 0;
};

void RecordMesh(
		VkCommandBuffer& cmdBuffer,
		const MeshGeometry& mesh,
		uint32_t			firstInstance,
		uint32_t			numInstances);