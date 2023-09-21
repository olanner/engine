#include "pch.h"

#define _DEVELOPMENT

#include <string>
#include <random>

#include "neat/General/Window.h"
#include "neat/Input/InputHandler.h"
#include "neat/General/MultiApplication.h"
#include "neat/General/Timer.h"

#include "Reflex.h"
#include "Glue/Glue.h"

#ifdef _DEBUG
#pragma comment(lib, "neat_Debugx64.lib")
#pragma comment(lib, "Reflex_Debugx64.lib")
#pragma comment(lib, "Glue_Debugx64.lib")
#else
#pragma comment(lib, "neat_Releasex64.lib")
#pragma comment(lib, "Reflex_Releasex64.lib")
#pragma comment(lib, "Glue_Releasex64.lib")
#endif

neat::InputHandler gInputHandler;
class RenderThread : public neat::Thread
{
public:
	RenderThread(void* hWND, const Vec2ui& windowRes)
		: myReflexInterface(GetThreadID())
	{
		myStartFunc = [this, hWND, windowRes]()
		{
			myReflexInterface.Start(hWND, windowRes, "vkdebug");
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

				if (gInputHandler.IsReleased('Q'))
				{
					myReflexInterface.ToggleFeature(rflx::Features::FEATURE_DEFERRED);
					myReflexInterface.ToggleFeature(rflx::Features::FEATURE_RAY_TRACING);
				}

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
		, myGlueInterface(GetThreadID())
	{
		myStartFunc = [this]()
		{
			myTimer.Start();
			myReflexInterface.Start(nullptr, {});
		};
		myMainFunc = [this]() -> void
		{
			myReflexInterface.BeginPush();
			myGlueInterface.Start();
			auto sceneHandle = myReflexInterface.CreateMesh("Assets/scene/scene.dae");
			auto skyBox = myReflexInterface.CreateImageCube("Assets/Cube Maps/stor_forsen.dds");
			skyBox.SetAsSkybox();

			std::vector<PixelValue> pixels;
			pixels.resize(16, { uint8_t(255), uint8_t(255), uint8_t(255), uint8_t(255) });
			auto alb = myReflexInterface.CreateImage(std::move(pixels));
			pixels = {};
			pixels.resize(16, { uint8_t(0), uint8_t(0), 0, 0 });
			auto mat = myReflexInterface.CreateImage(std::move(pixels));
			auto torus = myReflexInterface.CreateMesh("Assets/Sascha/torusknot.obj", { alb, mat });
			myReflexInterface.EndPush();

			while (IsRunning())
			{
				gInputHandler.BeginFrame();
				myReflexInterface.BeginPush();
				myGlueInterface.Tick();
				myTimer.Tick();
				auto dt = myTimer.GetDeltaTime();

				static float distance = 8.f;
				static float xRot = 0.f;
				static float yRot = 0.f;
				static float y = 1;

				y += float(gInputHandler.IsReleased(VK_SPACE));
				y -= float(gInputHandler.IsReleased(VK_SHIFT));

				distance -= gInputHandler.GetWheelDelta() * 128.f * dt;
				if (gInputHandler.IsHeld(VK_LBUTTON) && GetAsyncKeyState(VK_MENU))
				{
					auto [dx, dy] = gInputHandler.GetMousePosDelta();
					yRot += dx * 0.05f * distance * dt;
					xRot += -dy * 0.05f * distance * dt;
				}
				distance += gInputHandler.IsHeld(VK_UP) * 128.f * dt;
				distance -= gInputHandler.IsHeld(VK_DOWN) * 128.f * dt;
				myReflexInterface.SetView({ 0, y, 0 }, { xRot, yRot }, distance);

				const auto tt = myTimer.GetTotalTime();
				myReflexInterface.PushRenderCommand(torus, { 0.5,2,0 }, { 0.04f,0.04f,0.04f }, { 0,1,0 }, tt * 3.14f * 0.66f);
				myReflexInterface.PushRenderCommand(sceneHandle);

				auto fps = std::to_string(int(1.f / dt));
				myReflexInterface.PushRenderCommand(
					FontID(0),
					fps.c_str(),
					{ -.99, 1, 0 },
					.08f,
					{ 0,1,1,1 });

				myReflexInterface.EndPush();
				gInputHandler.EndFrame();
				const auto count = GetTickCount64();
				while ((GetTickCount64() - count) < 5)
				{
				}
			}
		};
	}

private:
	rflx::Reflex	myReflexInterface;
	glui::Glue		myGlueInterface;
	Timer			myTimer;

};

bool OnWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	hwnd;
	return gInputHandler.TakeMessages(msg, wParam, lParam);
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


	{
		std::vector<std::unique_ptr<neat::Thread>> threads;
		threads.emplace_back(std::make_unique<RenderThread>(
			window.GetWindowHandle(),
			Vec2ui{window.GetWindowParameters().width, window.GetWindowParameters().height}));
		threads.emplace_back(std::make_unique<LogicThread>(threads.front()->GetNextSignal()));
		neat::MultiApplication app(
			window, std::move(threads)
		);
		retVal = int(app.Loop());
	}


#ifdef _DEBUG
	if (std::string(_bstr_t(lpCmdLine)).find("vkdebug") != std::string::npos)
	{
		system("pause");
	}
#endif
	return retVal;
}