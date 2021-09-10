
#pragma once

namespace Box
{
	struct Box
	{
		rflx::ImageHandle	nineSlice;
		uint32_t			width;
		uint32_t			height;

	};
	
	void
	AddBox(
		const char* imgPath,
		Vec2ui size);

	void
	Tick(
		float dt, 
		float tt, 
		int fnr);

}
