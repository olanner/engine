
#pragma once

struct Mesh
{
	VkDescriptorBufferInfo vertexInfo;
	VkDescriptorBufferInfo indexInfo;
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
	uint32_t numVertices;
	uint32_t numIndices;
};

void RecordMesh(
		VkCommandBuffer& cmdBuffer,
		const Mesh& mesh,
		uint32_t			firstInstance,
		uint32_t			numInstances);