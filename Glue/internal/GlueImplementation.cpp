#include "pch.h"
#include "GlueImplementation.h"
#include "Systems/SlicedSpriteSystem.h"
#include "Components/SlicedSpriteComponent.h"

glui::GlueImplementation::GlueImplementation(
	neat::ThreadID threadID)
	: myReflex(threadID)
{
	mySlicedSpriteComponents = std::make_shared<ComponentsSlicedSprite>(myReflex);
	mySlicedSpriteSystem = std::make_shared<SlicedSpriteSystem>(myReflex, *mySlicedSpriteComponents);

	mySlicedSpriteComponents->AddComponent(EntityID(0), "Assets/Textures/element.tga", {4.f, 4.f});
}

void
glui::GlueImplementation::Tick()
{
	mySlicedSpriteSystem->Tick(0);
}
