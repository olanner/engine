
#pragma once

#include "Reflex/WindowInfo/WindowInfo.h"
#include "Reflex/identities.h"
#include "Reflex/VK/Mesh/MeshHandle.h"
#include "Reflex/VK/Image/CubeHandle.h"
#include "Reflex/VK/Image/ImageHandle.h"

#include "glm/glm.hpp"

using Vec2f = glm::vec2;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Mat3f = glm::mat3x3;
using Mat4f = glm::mat4x4;

class Reflex
{
public:
			Reflex(const WindowInfo& windowInformation, const char* cmdArgs = nullptr);
			~Reflex();

	bool	IsGood();
	void	BeginFrame();
	void	Submit();
	void	EndFrame();

private:
	class	VulkanImplementation* myVKImplementation;
	bool	myIsGood = false;

};

namespace rflx
{
	void		SetUseRayTracing(bool useFlag);

	void		BeginPush();
	void		PushRenderCommand(
					const MeshHandle&	handle,
					const Vec3f&		position = {0,0,0},
					const Vec3f&		scale = {1,1,1},
					const Vec3f&		forward = {0,0,1},
					float				rotation = 0);
	void		PushRenderCommand(
					FontID			fontID,
					const char*		text,
					const Vec3f&	position,
					float			scale,
					const Vec4f&	color);
	void		EndPush();

	void		SetView(
					const Vec3f& position, 
					const Vec2f& rotation, 
					float distance);
}