
#pragma once

namespace glui
{
	class Glue
	{
	public:
				Glue();
				~Glue();

		void	Tick(
					float dt, 
					float tt, 
					int fnr);

	private:
		
		
	};
}

namespace glui
{
	void	Box(
				const char* path,
				uint32_t width,
				uint32_t height);
	
}