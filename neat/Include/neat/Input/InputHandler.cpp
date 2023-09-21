#include "pch.h"
#include "InputHandler.h"
#include <Windowsx.h>

bool neat::InputHandler::TakeMessages( UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
		case WM_KEYDOWN:
			myKeyStates[wParam] = true;
			return true;
		case WM_KEYUP:
			myKeyStates[wParam] = false;
			return true;
		case WM_MOUSEMOVE:
			myMPosX = GET_X_LPARAM( lParam );
			myMPosY = GET_Y_LPARAM( lParam );
			return true;
		case WM_LBUTTONDOWN:
			myKeyStates[VK_LBUTTON] = true;
			return true;
		case WM_LBUTTONUP:
			myKeyStates[VK_LBUTTON] = false;
			return true;
		case WM_MBUTTONDOWN:
			myKeyStates[VK_MBUTTON] = true;
			return true;
		case WM_MBUTTONUP:
			myKeyStates[VK_MBUTTON] = false;
			return true;
		case WM_RBUTTONDOWN:
			myKeyStates[VK_RBUTTON] = true;
			return true;
		case WM_RBUTTONUP:
			myKeyStates[VK_RBUTTON] = false;
			return true;
		case WM_MOUSEWHEEL:
			myWheelDelta = GET_WHEEL_DELTA_WPARAM( wParam ) ;
			myWheelDelta = 2 * ( ( myWheelDelta - -120 ) / (120 - -120) ) - 1;
			return true;

		default: 
		return false;
	}

	return true;
}

void neat::InputHandler::BeginFrame()
{

}

void neat::InputHandler::EndFrame()
{
	myWheelDelta = 0.f;
	myLastKeyStates = myKeyStates;
	myLastMPosX = myMPosX;
	myLastMPosY = myMPosY;
}

void neat::InputHandler::SetWindowSize( float x, float y )
{
	myWindowX = x;
	myWindowY = y;
}

bool neat::InputHandler::IsHeld( int key )
{
	return myKeyStates[key];
}

bool neat::InputHandler::IsPressed( int key )
{
	return !myLastKeyStates[key] && myKeyStates[key];
}

bool neat::InputHandler::IsReleased( int key )
{
	return myLastKeyStates[key] && !myKeyStates[key];
}

float neat::InputHandler::GetWheelDelta()
{
	return myWheelDelta;
}

std::tuple<float, float> neat::InputHandler::GetMousePos()
{
	return { myMPosX, myMPosY };
}

std::tuple<float, float> neat::InputHandler::GetMousePosDelta()
{
	float dx = myLastMPosX - myMPosX;
	float dy = myLastMPosY - myMPosY;
	return { dx,dy };
}

std::tuple<float, float> neat::InputHandler::GetScreenMousePos()
{
	POINT p;
	GetCursorPos( &p );
	return { float( p.x ), float( p.y ) };
}

std::tuple<float, float> neat::InputHandler::GetNormalMousePos()
{
	if ( myWindowX == 0 )
	{
		return { 0,0 };
	}
	return { myMPosX / myWindowX, myMPosY / myWindowY };
}