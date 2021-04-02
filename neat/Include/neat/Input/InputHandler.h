
#pragma once

#include <Windows.h>
#include <Windowsx.h>
#include <map>
#include <array>

class InputHandler
{
public:
	static bool TakeMessages( UINT msg, WPARAM wParam, LPARAM lParam );
	static void BeginFrame();
	static void EndFrame();

	static void SetWindowSize(float x, float y);

	static bool IsHeld(int key);
	//static bool GetLastKeyState( int key );
	static bool IsPressed( int key );
	static bool IsReleased( int key );
	
	static float GetWheelDelta();
	static std::tuple<float,float> GetMousePos();
	static std::tuple<float,float> GetNormalMousePos(); 
	static std::tuple<float,float> GetMousePosDelta(); 
	static std::tuple<float,float> GetScreenMousePos();

private:
	static float ourWheelDelta;
	static std::array<bool, 256> ourKeyStates;
	static std::array<bool, 256> ourLastKeyStates;

	//static std::map<int, bool> ourKeyStates;
	//static std::map<int, bool> ourLastKeyStates;

	static float ourMPosX;
	static float ourMPosY;
	
	static float ourLastMPosX;
	static float ourLastMPosY;

	static float ourWindowX;
	static float ourWindowY;


};