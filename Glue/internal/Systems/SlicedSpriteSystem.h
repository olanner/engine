#pragma once

namespace glui
{	
class SlicedSpriteSystem
{
public:
	SlicedSpriteSystem(
		rflx::Reflex&					reflex,
		class ComponentsSlicedSprite&	components);

	void Tick(float dt);
	
private:
	rflx::Reflex&			theirReflex;
	ComponentsSlicedSprite& theirComponents;
};
}