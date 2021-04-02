
#pragma once
#include "Reflex/VK/Pipelines/Pipeline.h"
#include "Reflex/VK/RenderPass/RenderPass.h"
#include "Reflex/VK/WorkerSystem/WorkerSystem.h"

struct TextRenderCommand
{
	std::array<char, 128>	text;
	Vec4f					color;
	Vec2f					position;
	float					scale;
	FontID					fontID;
	uint32_t				numCharacters;
};

struct GlyphInstance
{
	Vec4f padding0;
	Vec4f color;
	Vec2f pos;
	Vec2f pivot;
	float scale;
	float fontID;
	float glyphIndex;
	float padding2;
};

class TextRenderer : public WorkerSystem
{
public:
													TextRenderer(class VulkanFramework&		vulkanFramework,
																 class SceneGlobals&		sceneGlobals,
																 class FontHandler&			fontHandler,
																 class RenderPassFactory&	renderPassFactory,
																 class UniformHandler&		uniformHandler,
																 QueueFamilyIndex* 			firstOwner, 
																 uint32_t					numOwners,
																 uint32_t					cmdBufferFamily);

	std::tuple<VkSubmitInfo, VkFence>				RecordSubmit(uint32_t					swapchainImageIndex,
																 VkSemaphore*				waitSemaphores,
																 uint32_t					numWaitSemaphores,
																 VkPipelineStageFlags*		waitPipelineStages,
																 uint32_t					numWaitStages,
																 VkSemaphore*				signalSemaphore) override;

	void											BeginPush();
	void											PushRenderCommand(
														const TextRenderCommand &			textRenderCommand);
	void											EndPush();

private:
	VulkanFramework&								theirVulkanFramework;
	SceneGlobals&									theirSceneGlobals;
	FontHandler&									theirFontHandler;
	UniformHandler&									theirUniformHandler;

	neat::static_vector<QueueFamilyIndex, 8>		myOwners;

	RenderPass										myRenderPass;

	class Shader*									myTextShader;
	Pipeline										myTextPipeline;

	std::array<VkCommandBuffer, NumSwapchainImages> myCmdBuffers;
	std::array<VkFence, NumSwapchainImages>			myCmdBufferFences;

	UniformID										myGlyphInstancesID;

	std::unordered_map<char, uint32_t>				myCharToGlyphIndex;
	
	std::mutex										mySwapMutex;
	uint8_t											myPushIndex = 0;
	uint8_t											myFreeIndex = 1;
	uint8_t											myRecordIndex = 2;
	std::array<neat::static_vector<TextRenderCommand, 8>, 3>
													myRenderCommands{};
	
};
