#include "pch.h"

#include "Application.h"

#include "Window.h"
#include "Timer.h"


Application::Application( Window& window, const std::function<void( float, float, int )>&& tickFunction ) :
	myTickFunction( tickFunction ),
	myWindow( window )
{
}

Application::~Application()
{
}

WPARAM Application::Loop()
{
	int fnr = 0;
	Timer timer;
	timer.Start();
	while ( true )
	{
		timer.Tick();

		MSG msg = myWindow.HandleMessages();
		if ( msg.message == WM_QUIT )
		{
			return msg.wParam;
		}
		myTickFunction( timer.GetDeltaTime(), timer.GetTotalTime(), fnr++ );
	}


}

HRESULT Application::Init()
{
	return S_OK;
}

void Application::Shutdown()
{
	PostQuitMessage(0);
}
