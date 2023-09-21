#pragma once

namespace glui
{
class GlueImplementation
{
public:
	GlueImplementation(
		neat::ThreadID threadID);

	void Tick();

private:
	rflx::Reflex myReflex;
	std::shared_ptr<class ComponentsSlicedSprite>	mySlicedSpriteComponents;
	std::shared_ptr<class SlicedSpriteSystem>		mySlicedSpriteSystem;
	
};
}