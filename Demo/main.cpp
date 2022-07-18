#include "pch.h"

#define _DEVELOPMENT

#include <string>
#include <random>

#include "neat/General/Window.h"
#include "neat/Input/InputHandler.h"
#include "neat/General/MultiApplication.h"
#include "neat/General/Timer.h"

#include "Reflex/Reflex.h"

#ifdef _DEBUG
#pragma comment(lib, "neat_Debugx64.lib")
#pragma comment(lib, "Reflex_Debugx64.lib")
#else
#pragma comment(lib, "neat_Releasex64.lib")
#pragma comment(lib, "Reflex_Releasex64.lib")
#endif

class RenderThread : public neat::Thread
{
public:
	RenderThread(const WindowInfo& windowInfo)
		: myReflexInterface(GetThreadID())
	{
		myStartFunc = [this, windowInfo]()
		{
			myReflexInterface.Start(windowInfo, "vkdebug");
			myTimer.Start();
		};
		myMainFunc = [this]()
		{
			while (IsRunning())
			{
				myTimer.Tick();
				auto dt = myTimer.GetDeltaTime();

				myReflexInterface.BeginPush();
				auto fps = std::to_string(int(1.f / dt));
				myReflexInterface.PushRenderCommand(
					FontID(0),
					fps.c_str(),
					{ -.99, .89, 0 },
					.08f,
					{ 0,1,1,1 });
				myReflexInterface.EndPush();

				myReflexInterface.BeginFrame();
				myReflexInterface.Submit();
				myReflexInterface.EndFrame();
			}
		};
	}

private:
	rflx::Reflex	myReflexInterface;
	Timer			myTimer;
};

class LogicThread : public neat::Thread
{
public:
	LogicThread(
		const std::shared_ptr<std::binary_semaphore>& semaphore)
		: Thread(semaphore)
		, myReflexInterface(GetThreadID())
	{
		myStartFunc = [this]()
		{
			myTimer.Start();
			myReflexInterface.Start({});
		};
		myMainFunc = [this]() -> void
		{
			auto boyHandle = rflx::CreateMesh("Assets/test_boy/test_boy.fbx");
			while (IsRunning())
			{
				InputHandler::BeginFrame();
				Sleep(1000 / 60);
				myTimer.Tick();
				auto dt = myTimer.GetDeltaTime();

				static float distance = 8.f;
				static float xRot = 0.f;
				static float yRot = 0.f;
				static float y = 1;

				y += InputHandler::IsReleased(VK_SPACE);
				y -= InputHandler::IsReleased(VK_SHIFT);

				distance -= InputHandler::GetWheelDelta() * 128.f * dt;
				if (InputHandler::IsHeld(VK_LBUTTON) && GetAsyncKeyState(VK_MENU))
				{
					auto [dx, dy] = InputHandler::GetMousePosDelta();
					yRot += dx * 0.05f * distance * dt;
					xRot += -dy * 0.05f * distance * dt;
				}
				distance += InputHandler::IsHeld(VK_UP) * 128.f * dt;
				distance -= InputHandler::IsHeld(VK_DOWN) * 128.f * dt;
				myReflexInterface.SetView({ 0, y,0 }, { xRot, yRot }, distance);

				myReflexInterface.PushRenderCommand(boyHandle);
				
				InputHandler::EndFrame();
			}
		};
	}

private:
	rflx::Reflex	myReflexInterface;
	Timer			myTimer;

};

bool OnWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	hwnd;
	return InputHandler::TakeMessages(msg, wParam, lParam);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	int retVal = 0;
	hPrevInstance;
	neat::Window window(hInstance, nCmdShow, { L"Demo", 1920, 1080, true, OnWinProc });
#ifdef _DEVELOPMENT
	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(h, FOREGROUND_GREEN | FOREGROUND_BLUE);
	::SetWindowPos(GetConsoleWindow(),       // handle to window
		HWND_TOPMOST,  // placement-order handle
		0,     // horizontal position
		0,      // vertical position
		640,  // width
		480, // height
		SWP_SHOWWINDOW);
#endif

	WindowInfo info = { window.GetWindowHandle(), uint32_t(window.GetWindowParameters().width), uint32_t(window.GetWindowParameters().height) };

	std::vector<std::unique_ptr<neat::Thread>> threads;
	threads.emplace_back(std::make_unique<RenderThread>(info));
	//threads.emplace_back(std::make_unique<LogicThread>(threads.front()->GetNextSignal()));
	neat::MultiApplication app(
		window, std::move(threads)
	);

	retVal = int(app.Loop());

#ifdef _DEBUG
	if (std::string(_bstr_t(lpCmdLine)).find("vkdebug") != std::string::npos)
	{
		//system("pause");
	}
#endif
	return retVal;
}