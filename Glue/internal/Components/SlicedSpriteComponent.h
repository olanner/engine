#pragma once

#include "Handles/ImageHandle.h"
#include "internal/Entity.h"

namespace glui
{
struct SlicedSpriteComponent
{
	std::shared_ptr<rflx::ImageHandle>	imgHandle;
	float								posX = 0;
	float								posY = 0;
	float								sizeX = 2;
	float								sizeY = 2;
};

class ComponentsSlicedSprite
{
	friend class SlicedSpriteSystem;
public:
				ComponentsSlicedSprite(
					rflx::Reflex& reflex);
	
	void		AddComponent(
					EntityID			id,
					const std::string&	imagePath,
					Vec2f				startSize = {2,2});
	void		RemoveComponent(
					EntityID id);

private:
	rflx::Reflex& myReflex;
	
	std::vector<EntityID>
				myActiveComponents;
	std::array<SlicedSpriteComponent, 128>
				myComponents = {};
};
}