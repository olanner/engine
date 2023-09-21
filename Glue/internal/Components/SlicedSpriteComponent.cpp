#include "pch.h"
#include "SlicedSpriteComponent.h"

glui::ComponentsSlicedSprite::ComponentsSlicedSprite(
	rflx::Reflex& reflex)
	: myReflex(reflex)
{
}

void
glui::ComponentsSlicedSprite::AddComponent(
	EntityID			id,
	const std::string&	imagePath, 
	Vec2f				startSize)
{
	myActiveComponents.emplace_back(id);
	auto& component = myComponents[int(id)] = {};
	component.sizeX = startSize.x;
	component.sizeY = startSize.y;
	component.imgHandle = std::make_shared<rflx::ImageHandle>(myReflex.CreateImage(imagePath, {3,3}));
}

void
glui::ComponentsSlicedSprite::RemoveComponent(
	EntityID id)
{
	if (
		const auto iter = std::ranges::find(myActiveComponents, id); 
		iter != myActiveComponents.end())
	{
		myActiveComponents.erase(iter);
	}
}
