
#include "pch.h"

#include "Window.h"

#include <cassert>

std::function<bool( HWND, UINT, WPARAM, LPARAM )> Window::ourOnWinProc;

Window::Window( HINSTANCE hInstance, int nCmdShow, const WindowParams& parameters ) :
	myHandle( nullptr ),
	myWindowClass(),
	myWindowParams( parameters )
{
	ZeroMemory( &myWindowClass, sizeof( WNDCLASSEX ) );

	myWindowClass.cbSize = sizeof( WNDCLASSEX );
	myWindowClass.style = CS_HREDRAW | CS_VREDRAW;
	myWindowClass.lpfnWndProc = WinProc;
	myWindowClass.hInstance = hInstance;
	myWindowClass.hCursor = LoadCursor( NULL, IDC_ARROW );
	myWindowClass.hbrBackground = (HBRUSH) COLOR_WINDOW;
	myWindowClass.lpszClassName = L"WindowClass1";

	DWORD windowType = WS_OVERLAPPEDWINDOW;
	if ( parameters.windowedFullscreenFlag )
	{
		windowType = WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_POPUP | WS_VISIBLE;
	}

	int screen_x = GetSystemMetrics( SM_CXSCREEN );
	int screen_y = GetSystemMetrics( SM_CYSCREEN );

	int xPos = screen_x / 2 - myWindowParams.width / 2;
	int yPos = screen_y / 2 - myWindowParams.height / 2;

	RECT clientRect = {
		0,
		0,
		myWindowParams.width,
		myWindowParams.height };

	AdjustWindowRect( &clientRect, windowType, FALSE );

	assert( parameters.onWinProc != nullptr );
	ourOnWinProc = parameters.onWinProc;

	RegisterClassEx( &myWindowClass );
	myHandle = CreateWindowEx( NULL,
								  L"WindowClass1",
								  myWindowParams.title,
								  windowType,
								  xPos,
								  yPos,
								  clientRect.right - clientRect.left,
								  clientRect.bottom - clientRect.top,
								  NULL,
								  NULL,
								  hInstance,
								  NULL );




	ShowWindow( myHandle, nCmdShow );

}

LRESULT Window::WinProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch ( message )
	{
		case WM_DESTROY:
			PostQuitMessage( 0 );
			return 0;
	}

	if ( !ourOnWinProc( hwnd, message, wParam, lParam ) )
	{
		// GÖR NÅNTING
	}

	return DefWindowProc( hwnd, message, wParam, lParam );
}

MSG Window::HandleMessages()
{
	MSG msg = { 0 };
	while ( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
	{
		TranslateMessage( &msg );
		DispatchMessage( &msg );
		if ( msg.message == WM_QUIT )
		{
			return msg;
		}
		for (auto& callback : myListenCallbacks)
		{
			callback(msg);
		}
	}
	return msg;
}

HWND Window::GetWindowHandle() const
{
	return myHandle;
}

WindowParams Window::GetWindowParameters() const
{
	return myWindowParams;
}

void Window::RegisterMSGCallback(MSGListenCallback callback)
{
	myListenCallbacks.emplace_back(std::move(callback));
}
