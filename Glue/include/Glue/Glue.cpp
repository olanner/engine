#include "pch.h"
#include "Glue.h"
#include "internal/GlueImplementation.h"

uint32_t	glui::Glue::ourUses = 0;
glui::GlueImplementation*
			glui::Glue::ourGlueImplementation = nullptr;

glui::Glue::Glue(
	neat::ThreadID threadID)
	: myThreadID(threadID)
{
	ourUses++;
}

glui::Glue::~Glue()
{
	ourUses--;
	if (ourUses == 0)
	{
		delete ourGlueImplementation;
		ourGlueImplementation = nullptr;
	}
}

void glui::Glue::Start()
{
	if (ourGlueImplementation != nullptr)
	{
		return;
	}
	ourGlueImplementation = new GlueImplementation(myThreadID);
}

void
glui::Glue::Tick()
{
	ourGlueImplementation->Tick();
}
