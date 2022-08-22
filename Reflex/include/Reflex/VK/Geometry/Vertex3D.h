#pragma once

struct Vertex3D
{
	Vec4f position;
	Vec4f normal;
	Vec4f tangent;
	Vec2f uv;
	Vec2f filler;
	Vec4f texIDs;
};

constexpr VkPipelineInputAssemblyStateCreateInfo Vertex3DIAInfo
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,

	.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	.primitiveRestartEnable = false,
	
};

constexpr VkVertexInputBindingDescription Vertex3DBinding =
{
	.binding = 0,
	.stride = sizeof Vertex3D,
	.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
};

constexpr int Vertex3DAttributesCount = 6;
constexpr VkVertexInputAttributeDescription Vertex3DAttributeDescriptions[Vertex3DAttributesCount]
{
	{
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = offsetof(Vertex3D, position)
	},
	{
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = offsetof(Vertex3D, normal)
	},
	{
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = offsetof(Vertex3D, tangent)
	},
	
	{
		.location = 3,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex3D, uv)
	},
	{
		.location = 4,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex3D, filler)
	},
	{
		.location = 5,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32A32_SFLOAT,
		.offset = offsetof(Vertex3D, texIDs)
	}
	
};

constexpr VkPipelineVertexInputStateCreateInfo Vertex3DInputInfo{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,
	
	.vertexBindingDescriptionCount = 1,
	.pVertexBindingDescriptions = &Vertex3DBinding, // Optional
	.vertexAttributeDescriptionCount = Vertex3DAttributesCount,
	.pVertexAttributeDescriptions = Vertex3DAttributeDescriptions, // Optional
};


