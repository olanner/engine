#include "pch.h"
#include "Mesh.h"

void
RecordMesh(
	VkCommandBuffer&	cmdBuffer,
	const Mesh&			mesh,
	uint32_t			firstInstance,
	uint32_t			numInstances)
{
	static VkDeviceSize offsets[]{0};
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &mesh.vertexBuffer, offsets);
	vkCmdBindIndexBuffer(cmdBuffer, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(cmdBuffer, mesh.numIndices, numInstances, 0, 0, firstInstance);
}
