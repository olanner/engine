#include "pch.h"
#include "InputHandler.h"

float InputHandler::ourWheelDelta = 0;

std::array<bool, 256> InputHandler::ourKeyStates{};
std::array<bool, 256> InputHandler::ourLastKeyStates{};

float InputHandler::ourMPosX = 0;
float InputHandler::ourMPosY = 0;
float InputHandler::ourLastMPosX = 0;
float InputHandler::ourLastMPosY = 0;

float InputHandler::ourWindowX = 0;
float InputHandler::ourWindowY = 0;

bool InputHandler::TakeMessages( UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch ( msg )
	{
		case WM_KEYDOWN:
			ourKeyStates[wParam] = true;
			return true;
		case WM_KEYUP:
			ourKeyStates[wParam] = false;
			return true;
		case WM_MOUSEMOVE:
			ourMPosX = GET_X_LPARAM( lParam );
			ourMPosY = GET_Y_LPARAM( lParam );
			return true;
		case WM_LBUTTONDOWN:
			ourKeyStates[VK_LBUTTON] = true;
			return true;
		case WM_LBUTTONUP:
			ourKeyStates[VK_LBUTTON] = false;
			return true;
		case WM_MBUTTONDOWN:
			ourKeyStates[VK_MBUTTON] = true;
			return true;
		case WM_MBUTTONUP:
			ourKeyStates[VK_MBUTTON] = false;
			return true;
		case WM_RBUTTONDOWN:
			ourKeyStates[VK_RBUTTON] = true;
			return true;
		case WM_RBUTTONUP:
			ourKeyStates[VK_RBUTTON] = false;
			return true;
		case WM_MOUSEWHEEL:
			ourWheelDelta = GET_WHEEL_DELTA_WPARAM( wParam ) ;
			ourWheelDelta = 2 * ( ( ourWheelDelta - -120 ) / (120 - -120) ) - 1;
			return true;

		default: 
		return false;
	}

	return true;
}

void InputHandler::BeginFrame()
{

}

void InputHandler::EndFrame()
{
	ourWheelDelta = 0.f;
	ourLastKeyStates = ourKeyStates;
	ourLastMPosX = ourMPosX;
	ourLastMPosY = ourMPosY;
}

void InputHandler::SetWindowSize( float x, float y )
{
	ourWindowX = x;
	ourWindowY = y;
}

bool InputHandler::IsHeld( int key )
{
	return ourKeyStates[key];
}

bool InputHandler::IsPressed( int key )
{
	return !ourLastKeyStates[key] && ourKeyStates[key];
}

bool InputHandler::IsReleased( int key )
{
	return ourLastKeyStates[key] && !ourKeyStates[key];
}

float InputHandler::GetWheelDelta()
{
	return ourWheelDelta;
}

std::tuple<float, float> InputHandler::GetMousePos()
{
	return { ourMPosX, ourMPosY };
}

std::tuple<float, float> InputHandler::GetMousePosDelta()
{
	float dx = ourLastMPosX - ourMPosX;
	float dy = ourLastMPosY - ourMPosY;
	return { dx,dy };
}

std::tuple<float, float> InputHandler::GetScreenMousePos()
{
	POINT p;
	GetCursorPos( &p );
	return { float( p.x ), float( p.y ) };
}

std::tuple<float, float> InputHandler::GetNormalMousePos()
{
	if ( ourWindowX == 0 )
	{
		return { 0,0 };
	}
	return { ourMPosX / ourWindowX, ourMPosY / ourWindowY };
}