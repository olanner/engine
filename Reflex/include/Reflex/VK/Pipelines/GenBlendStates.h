#pragma once

constexpr VkPipelineColorBlendAttachmentState AlphaBlending
{
	.blendEnable = VK_TRUE,
	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
};

constexpr VkPipelineColorBlendAttachmentState DisabledBlending
{
	.blendEnable = VK_FALSE,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
};