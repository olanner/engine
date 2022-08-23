
#pragma once

#include "glm/glm.hpp"
using Vec2f = glm::vec2;
using Vec2ui = glm::vec<2, uint32_t>;
using Vec3f = glm::vec3;
using Vec4f = glm::vec4;
using Mat3f = glm::mat3x3;
using Mat4f = glm::mat4x4;

#include "neat/General/Thread.h"
#include "RFVK/Features.h"
#include "RFVK/Misc/Identities.h"
#include "Handles/MeshHandle.h"
#include "Handles/CubeHandle.h"
#include "Handles/ImageHandle.h"

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
											void*			hWND, 
											const Vec2ui&	windowRes, 
											const char*		cmdArgs = nullptr);
		void							ToggleFeature(Features feature);
		
		void							BeginFrame();
		void							Submit();
		void							EndFrame();

		void							BeginPush();
		void							EndPush();
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

		void							SetView(
											const Vec3f&	position,
											const Vec2f&	rotation,
											float			distance);
		void							SetScaleReference(
											ImageHandle		handle,
											Vec2f			coefficient);

		neat::ThreadID					GetThreadID() const;
		
		CubeHandle						CreateImageCube(const char* path);
		ImageHandle						CreateImage(
											const char* path, 
											Vec2f tiling = { 1,1 });
		ImageHandle						CreateImage(
											std::vector<PixelValue>&& pixelData, 
											Vec2f tiling = { 1,1 });
		MeshHandle						CreateMesh(
											const char* path, 
											std::vector<class ImageHandle>&& imgHandles = {});
	
	private:
		neat::ThreadID					myThreadID;
		Vec2f							my2DScaleRef = {};

	};
}