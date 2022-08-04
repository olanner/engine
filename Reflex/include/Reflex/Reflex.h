
#pragma once

#include "glm/glm.hpp"
using Vec2f = glm::vec2;
using Vec2ui = glm::vec<2, uint32_t>;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Mat3f = glm::mat3x3;

#include "neat/General/Thread.h"
#include "Reflex/WindowInfo/WindowInfo.h"
#include "Reflex/identities.h"
#include "Reflex/VK/Mesh/MeshHandle.h"
#include "Reflex/VK/Image/CubeHandle.h"
#include "Reflex/VK/Image/ImageHandle.h"


using Mat4f = glm::mat4x4;


class VulkanImplementation;
namespace rflx
{
	class Reflex
	{
		static uint32_t					ourUses;
		static VulkanImplementation*	ourVKImplementation;
	public:
										Reflex(neat::ThreadID threadID);
										~Reflex();

		bool							Start(
											const ::WindowInfo&	windowInformation, 
											const char*			cmdArgs = nullptr);
		void							BeginFrame();
		void							Submit();
		void							EndFrame();

		void							SetUseRayTracing(bool useFlag);

		void							BeginPush();
		void							PushRenderCommand(
											const MeshHandle& handle,
											const Vec3f& position = { 0,0,0 },
											const Vec3f& scale = { 1,1,1 },
											const Vec3f& forward = { 0,0,1 },
											float				rotation = 0);
		void							PushRenderCommand(
											FontID			fontID,
											const char* text,
											const Vec3f& position,
											float			scale,
											const Vec4f& color);
		void							PushRenderCommand(
											ImageHandle		handle,
											uint32_t		subImg,
											Vec2f			position,
											Vec2f			scale,
											Vec2f			pivot = { 0,0 },
											Vec4f			color = { 1,1,1,1 });
		void							EndPush();

		void							SetView(
											const Vec3f&	position,
											const Vec2f&	rotation,
											float			distance);
		void							SetScaleReference(
											ImageHandle		handle,
											Vec2f			coefficient);

		CubeHandle						CreateImageCube(const char* path) const;
		ImageHandle						CreateImage(const char* path, Vec2f tiling = { 1,1 }) const;
		ImageHandle						CreateImage(std::vector<PixelValue>&& pixelData, Vec2f tiling = { 1,1 }) const;
		MeshHandle						CreateMesh(const char* path, std::vector<class ImageHandle>&& imgHandles = {});
	
	private:
		neat::ThreadID					myThreadID;
		Vec2f							my2DScaleRef = {};

	};
}