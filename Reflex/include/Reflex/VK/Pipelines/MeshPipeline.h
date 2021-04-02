
#pragma once

#include "Reflex/VK/Geometry/Vertex3D.h"

constexpr VkPipelineRasterizationStateCreateInfo MeshRasterState
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,
	.depthClampEnable = VK_FALSE,
	.rasterizerDiscardEnable = VK_FALSE,
	.polygonMode = VK_POLYGON_MODE_FILL,
	.cullMode = VK_CULL_MODE_BACK_BIT,
	.frontFace = VK_FRONT_FACE_CLOCKWISE,
	.depthBiasEnable = VK_FALSE,
	.lineWidth = 1.f,
};

constexpr VkPipelineMultisampleStateCreateInfo MeshMSState
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,

	.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
	.sampleShadingEnable = VK_FALSE,
};

constexpr VkPipelineDepthStencilStateCreateInfo MeshDepthStencilState
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,

	.depthTestEnable = VK_TRUE,
	.depthWriteEnable = VK_TRUE,
	.depthCompareOp = VK_COMPARE_OP_LESS,
	.depthBoundsTestEnable = VK_FALSE,
	.stencilTestEnable = VK_FALSE,
	.front{},
	.back{},
	.minDepthBounds = 0.f,
	.maxDepthBounds = 1.f,
};

constexpr VkPipelineColorBlendAttachmentState MeshBlendStateAttachment
{
	.blendEnable = VK_TRUE,
	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
};

constexpr VkPipelineColorBlendStateCreateInfo MeshBlendState
{
	.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,
	.logicOpEnable = VK_FALSE,
	.attachmentCount = 1,
	.pAttachments = &MeshBlendStateAttachment,
};



constexpr VkGraphicsPipelineCreateInfo MeshPipelineState
{
	.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
	.pNext = nullptr,
	.flags = NULL,

	.stageCount = 0,    // REQ
	.pStages = nullptr, // REQ
	.pVertexInputState = &Vertex3DInputInfo,
	.pInputAssemblyState = &Vertex3DIAInfo,
	.pTessellationState = nullptr,
	.pViewportState = nullptr, // REQ
	.pRasterizationState = &MeshRasterState,
	.pMultisampleState = &MeshMSState,
	.pDepthStencilState = &MeshDepthStencilState,
	.pColorBlendState = &MeshBlendState,

};