#include "pch.h"
#include "SceneGlobals.h"

#include "Reflex/VK/VulkanFramework.h"
#include "Reflex/VK/Uniform/UniformHandler.h"

SceneGlobals::SceneGlobals(
	VulkanFramework&	vulkanFramework,
	UniformHandler&		uniformHandler,
	QueueFamilyIndex	transferFamily,
	QueueFamilyIndex	graphicsFamily,
	QueueFamilyIndex	computeFamily)
	: theirVulkanFramework(vulkanFramework)
	, theirUniformHandler(uniformHandler)
{
	// VIEW PROJECTION
	auto [tw, th] = theirVulkanFramework.GetTargetResolution();

	myGlobalsData.view = glm::lookAtLH(Vec3f(0.f, 0.f, -4.f), Vec3f(0.f, 0.f, 0.f), Vec3f(0.f, 1.f, 0.f));
	myGlobalsData.proj = glm::perspectiveFovLH_ZO(75.f / 180.f * 3.14f, tw, th, .001f, 1000.f);
	myGlobalsData.inverseView = glm::inverse(myGlobalsData.view);
	myGlobalsData.inverseProj = glm::inverse(myGlobalsData.proj);
	myGlobalsData.skyboxID = 0;
	myGlobalsData.resolution = {tw, th};

	QueueFamilyIndex indices[2]{graphicsFamily, transferFamily};
	myViewProjectionID = theirUniformHandler.RequestUniformBuffer(&myGlobalsData,
																   sizeof UniformGlobals);
	assert(!(BAD_ID(myViewProjectionID)) && "failed creating view projection uniform");

}

VkDescriptorSetLayout
SceneGlobals::GetGlobalsLayout() const
{
	auto [layout, set] = theirUniformHandler[myViewProjectionID];
	return layout;
}

void
SceneGlobals::BindGlobals(
	VkCommandBuffer		commandBuffer,
	VkPipelineLayout	pipelineLayout,
	uint32_t			setIndex,
	VkPipelineBindPoint	bindPoint)
{
	theirUniformHandler.UpdateUniformData(myViewProjectionID, &myGlobalsData);

	auto [layout, set] = theirUniformHandler[myViewProjectionID];
	vkCmdBindDescriptorSets(commandBuffer,
							 bindPoint,
							 pipelineLayout,
							 setIndex,
							 1,
							 &set,
							 0,
							 nullptr);

}

void
SceneGlobals::SetView(
	const Vec3f&	position,
	const Vec2f&	rotation,
	float			distance)
{
	Vec4f eye(0, 0, distance, 1.f);
	Mat4f rot = glm::identity<Mat4f>();
	rot *= glm::rotate(glm::identity<Mat4f>(), rotation.x, {1,0,0});
	rot *= glm::rotate(glm::identity<Mat4f>(), rotation.y, {0,1,0});
	eye = eye * rot;
	myGlobalsData.view = glm::lookAtLH(Vec3f(eye.x, eye.y, eye.z),
											   position,
											   Vec3f(0.f, 1.f, 0.));
	myGlobalsData.inverseView = glm::inverse(myGlobalsData.view);
}

void
SceneGlobals::SetSkybox(CubeID id)
{
	myGlobalsData.skyboxID = uint32_t(id);
}
