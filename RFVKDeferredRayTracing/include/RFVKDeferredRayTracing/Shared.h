
#pragma once
#include "RFVK/Mesh/MeshRenderCommand.h"
#include "RFVK/WorkerSystem/WorkScheduler.h"

struct GBuffer
{
	VkImageView				albedo;
	VkImageView				position;
	VkImageView				normal;
	VkImageView				material;
	VkDescriptorSetLayout	layout;
	VkDescriptorSet			set;
};

using MeshRenderSchedule = WorkScheduler<MeshRenderCommand, 1024, 1024>;
