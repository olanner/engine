
#pragma once

#define WIN32_LEAN_AND_MEAN

#include "WindowParams.h"

class Window
{
	static std::function<bool(HWND, UINT, WPARAM, LPARAM)> ourOnWinProc;
public:
	static LRESULT CALLBACK WinProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );
	
	Window( HINSTANCE hInstance, int nCmdShow, const WindowParams & parameters );


	MSG HandleMessages();

	HWND GetWindowHandle() const;
	WindowParams GetWindowParameters() const;

private:
	HWND myHandle;
	WNDCLASSEX myWindowClass;
	WindowParams myWindowParams;
};

