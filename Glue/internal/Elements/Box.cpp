#include "pch.h"
#include "Box.h"

neat::static_vector<Box::Box, 128> gBoxes;

void
Box::AddBox(
	const char* imgPath,
	Vec2ui		size)
{
	Box box =
	{ rflx::CreateImage(imgPath)
	,	size.x
	,	size.y };
	gBoxes.emplace_back(box);
}

void
Box::Tick(
	float	dt,
	float	tt,
	int		fnr)
{
	for (auto& box : gBoxes)
	{
		uint32_t len = box.width * box.height;
		for (uint32_t row = 0; row < box.width; ++row)
		{
			for (uint32_t col = 0; col < box.height; ++col)
			{
				uint32_t y = row / 9;
				uint32_t x = col / 9;
				uint32_t i = y * 9 + x;
				//rflx::PushRenderCommand(box.nineSlice, i, { 0,0 }, { 1,1 }, {-1,0});
			}
		}
	}
}
