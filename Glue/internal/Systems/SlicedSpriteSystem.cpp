#include "pch.h"
#include "SlicedSpriteSystem.h"
#include "internal/Components/SlicedSpriteComponent.h"

glui::SlicedSpriteSystem::SlicedSpriteSystem(
	rflx::Reflex&			reflex,
	ComponentsSlicedSprite& components)
	: theirReflex(reflex)
	, theirComponents(components)
{
}

void
glui::SlicedSpriteSystem::Tick(
	float dt)
{
	for (auto&& id : theirComponents.myActiveComponents)
	{
		auto& cmp = theirComponents.myComponents[int(id)];
		theirReflex.SetScaleReference(*cmp.imgHandle, {1.f, 1.f});

		const int iyMax = int(std::ceil(cmp.sizeY));
		const int ixMax = int(std::ceil(cmp.sizeX));
		Vec4f color = {0.65f, 0.78f, 0.57f, 1};
		theirReflex.PushRenderCommand(*cmp.imgHandle, 0, {cmp.posX, cmp.posY}, {1, 1}, {0, 0}, color);
		theirReflex.PushRenderCommand(*cmp.imgHandle, 2, {cmp.posX + float(ixMax - 1), cmp.posY}, {1, 1}, {0, 0}, color);
		theirReflex.PushRenderCommand(*cmp.imgHandle, 6, {cmp.posX, cmp.posY - float(iyMax - 1)}, {1, 1}, {0, 0}, color);
		theirReflex.PushRenderCommand(*cmp.imgHandle, 8, {cmp.posX + float(ixMax - 1), cmp.posY - float(iyMax - 1)}, {1, 1}, {0, 0}, color);
		for (int index = 1; index < ixMax - 1; ++index)
		{
			theirReflex.PushRenderCommand(*cmp.imgHandle, 1, {cmp.posX + float(index), cmp.posY}, {1, 1}, {0, 0}, color);
		}
		for (int index = 1; index < iyMax - 1; ++index)
		{
			theirReflex.PushRenderCommand(*cmp.imgHandle, 3, {cmp.posX, cmp.posY - float(index)}, {1, 1}, {0, 0}, color);
		}
		for (int index = 1; index < iyMax - 1; ++index)
		{
			theirReflex.PushRenderCommand(*cmp.imgHandle, 5, {cmp.posX + float(ixMax - 1), cmp.posY - float(index)}, {1, 1}, {0, 0}, color);
		}
		for (int index = 1; index < ixMax - 1; ++index)
		{
			theirReflex.PushRenderCommand(*cmp.imgHandle, 7, {cmp.posX + float(index), cmp.posY - float(iyMax - 1)}, {1, 1}, {0, 0}, color);
		}

		{
			const int xWidth = ixMax - 2;
			const int yWidth = iyMax - 2;
			const int numTiles = xWidth * yWidth;
			
			for (int index = 0; index < numTiles; ++index)
			{
				const int x = 1 + index % xWidth;
				const int y = 1 + index / yWidth;
				theirReflex.PushRenderCommand(*cmp.imgHandle, 4, {cmp.posX + float(x), cmp.posY - float(y)}, {1, 1}, {0, 0}, color);
			}
		}
		
		/*for (int x = 0; x < ixMax; ++x)
		{
			subImg += x == 1;
			subImg += x == ixMax - 1;
			theirReflex.PushRenderCommand(*cmp.imgHandle, subImg, {cmp.posX + float(x), cmp.posY}, {1, 1}, {}, {0,1,1,1});
		}
		int midMax = iyMax * ixMax - ixMax * 2;
		for (int i = 0; i < ixMax; ++i)
		{
			subImg += i == 1;
			subImg += i == ixMax - 1;
			theirReflex.PushRenderCommand(*cmp.imgHandle, subImg, {cmp.posX + float(i), cmp.posY}, {1, 1}, {}, {0,1,1,1});
		}*/
		/*for (int y = 0; y < iyMax; ++y)
		{
			subImg = 3 * (y != 0 && y != iyMax - 1);
			for (int x = 0; x < ixMax; ++x)
			{
				subImg += x == 1;
				subImg += x == ixMax - 1;
				theirReflex.PushRenderCommand(*cmp.imgHandle, subImg, {cmp.posX + float(x), cmp.posY - float(y)}, {1, 1});
			}
			subImg++;
			subImg += (iyMax == 2)*3;
		}*/
	}
}
