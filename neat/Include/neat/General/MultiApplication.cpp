#include "pch.h"
#include "MultiApplication.h"
#include <numeric>
#include "Timer.h"
#include "Window.h"
#include "neat/Input/InputHandler.h"
#include "Thread.h"

neat::MultiApplication::MultiApplication(
	Window&									window, 
	std::vector<std::unique_ptr<Thread>>&&	threads)
		:	myThreads(std::move(threads))
		,	myWindow(window)
{
}

neat::MultiApplication::~MultiApplication()
{
	for (auto& thread : myThreads)
	{
		while (!thread->Join());
	}
}

WPARAM neat::MultiApplication::Loop()
{
	for (auto& thread : myThreads)
	{
		thread->Start();
	}
	while (true)
	{
		std::this_thread::yield();

		MSG msg = myWindow.HandleMessages();
		if (msg.message == WM_QUIT)
		{
			for (auto& thread : myThreads)
			{
				while(!thread->Join());
			}
			return msg.wParam;
		}

	}
}

void neat::MultiApplication::Shutdown()
{
	PostQuitMessage(0);
}
