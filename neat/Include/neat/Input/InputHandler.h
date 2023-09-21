
#pragma once

#include <Windows.h>
#include <array>

namespace neat
{
	class InputHandler
	{
	public:
		bool						TakeMessages(UINT msg, WPARAM wParam, LPARAM lParam);
		void						BeginFrame();
		void						EndFrame();

		void						SetWindowSize(float x, float y);

		bool						IsHeld(int key);
		bool						IsPressed(int key);
		bool						IsReleased(int key);

		float						GetWheelDelta();
		std::tuple<float, float>	GetMousePos();
		std::tuple<float, float>	GetNormalMousePos();
		std::tuple<float, float>	GetMousePosDelta();
		std::tuple<float, float>	GetScreenMousePos();

	private:
		float					myWheelDelta = 0.f;
		std::array<bool, 256>	myKeyStates = {};
		std::array<bool, 256>	myLastKeyStates = {};

		float					myMPosX = 0;
		float					myMPosY = 0;

		float					myLastMPosX = 0;
		float					myLastMPosY = 0;

		float					myWindowX = 0;
		float					myWindowY = 0;


	};
}