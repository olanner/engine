#include "pch.h"
#include "MultiApplication.h"
#include <numeric>
#include "Timer.h"
#include "Window.h"
#include "neat/Input/InputHandler.h"

MultiApplication::MultiApplication( Window& window, TickFunc* tickFuncs, uint32_t numTickFuncs ) :
	myWindow(window)
{
	myIsRunnings.resize(numTickFuncs, true);
	for ( uint32_t i = 0; i < numTickFuncs; i++ )
	{
		auto funct = tickFuncs[i];
		int threadID = i;

		auto funcExt = [this, funct, threadID]()
		{
			Timer timer;
			timer.Start();
			int fnr = -1;
			while (myIsRunnings[threadID])
			{
				timer.Tick();
				funct(timer.GetDeltaTime(), timer.GetTotalTime(), ++fnr);
			}
		};

		myThreads.emplace_back(new std::thread( funcExt ));
	}
}

MultiApplication::~MultiApplication()
{
	for (int i = 0; i < int(myThreads.size()); ++i)
	{
		if ( myThreads[i] )
		{
			myIsRunnings[i] = false;
			myThreads[i]->join();
			SAFE_DELETE( myThreads[i] );
		}
	}
}

WPARAM MultiApplication::Loop()
{
	while ( true )
	{
		std::this_thread::yield();

		MSG msg = myWindow.HandleMessages();
		if ( msg.message == WM_QUIT )
		{
			for (int i = 0; i < int(myThreads.size()); ++i)
			{
				myIsRunnings[i] = false;
				myThreads[i]->join();
				SAFE_DELETE(myThreads[i]);
			}
			return msg.wParam;
		}
		
	}
}

void MultiApplication::Shutdown()
{
	PostQuitMessage(0);
}
