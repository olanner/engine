
#pragma once

struct UniformGlobals
{
	Mat4f		view;
	Mat4f		proj;
	Mat4f		inverseView;
	Mat4f		inverseProj;
	Vec2f		resolution;
	uint32_t	skyboxID;

};

class SceneGlobals
{
public:
							SceneGlobals(
								class VulkanFramework&	vulkanFramework,
								class UniformHandler&	uniformHandler,
								QueueFamilyIndex		transferFamily,
								QueueFamilyIndex		graphicsFamily,
								QueueFamilyIndex		computeFamily);

	VkDescriptorSetLayout	GetGlobalsLayout() const;
	void					BindGlobals(
								VkCommandBuffer		commandBuffer,
								VkPipelineLayout	pipelineLayout,
								uint32_t			setIndex,
								VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

	void					SetView(
							const Vec3f&	position, 
							const Vec2f&	rotation, 
							float			distance);
	void					SetSkybox(
								CubeID id);

private:
	VulkanFramework&		theirVulkanFramework;
	UniformHandler&			theirUniformHandler;

	UniformGlobals			myGlobalsData;
	UniformID				myViewProjectionID = UniformID(INVALID_ID);

};