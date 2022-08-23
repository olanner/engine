#pragma once

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