#pragma once

struct Vertex2D
{
	Vec4f position;
	Vec4f color;
	Vec2f uv;
};

constexpr VkPipelineInputAssemblyStateCreateInfo Vertex2DIAInfo
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,

	.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	.primitiveRestartEnable = false,

};

constexpr VkVertexInputBindingDescription Vertex2DBinding =
{
	.binding = 0,
	.stride = sizeof Vertex2D,
	.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
};

constexpr int Vertex2DAttributesCount = 3;
constexpr VkVertexInputAttributeDescription Vertex2DAttributeDescriptions[Vertex2DAttributesCount]
{
	{
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = offsetof(Vertex2D, position)
	},
	{
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = offsetof(Vertex2D, color)
	},
	{
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex2D, uv)
	}
};

constexpr VkPipelineVertexInputStateCreateInfo Vertex2DInputInfo{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,

	.vertexBindingDescriptionCount = 1,
	.pVertexBindingDescriptions = &Vertex2DBinding, // Optional
	.vertexAttributeDescriptionCount = Vertex2DAttributesCount,
	.pVertexAttributeDescriptions = Vertex2DAttributeDescriptions, // Optional
};
