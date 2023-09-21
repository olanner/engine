
#pragma once

namespace glui
{
	class GlueImplementation;
	class Glue
	{
		static	uint32_t	ourUses;
		static	GlueImplementation*
							ourGlueImplementation;
	public:
							Glue(neat::ThreadID threadID);
							~Glue();

		void				Start();
		void				Tick();

	private:
		neat::ThreadID		myThreadID;
		
	};
}