#include "pch.h"

#define _DEVELOPMENT

#include <string>
#include <random>

//#include "neat/General/Application.h"
#include "neat/General/MultiApplication.h"
#include "neat/General/Window.h"
#include "neat/Input/InputHandler.h"

#include "Reflex/Reflex.h"
#include "../Reflex/constexprs.h"
#include "neat/General/Application.h"

#ifdef _DEBUG
#pragma comment(lib, "Reflex_Debugx64.lib")
#else
#pragma comment(lib, "Reflex_Releasex64.lib")
#endif


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
	Window window(hInstance, nCmdShow, {L"Demo", 1920, 1080, true, OnWinProc});
#ifdef _DEVELOPMENT
		AllocConsole();
		freopen_s((FILE**) stdout, "CONOUT$", "w", stdout);
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
		WindowInfo info = {window.GetWindowHandle(), uint32_t(window.GetWindowParameters().width), uint32_t(window.GetWindowParameters().height)};

		rflx::Reflex reflexA;
		if (!reflexA.Start(info, _bstr_t(lpCmdLine)))
		{
			MessageBox(window.GetWindowHandle(), L"reflex failed to start", L"error", NULL);
			return 0;
		}
		rflx::Reflex reflexB;
	
		std::random_device rd;
		std::mt19937_64 mt(rd());
		std::uniform_int_distribution<int> dist(0, 256);

		auto boyHandle = rflx::CreateMesh("Assets/test_boy/test_boy.fbx");
	
		constexpr int gridDimX = 7;
		constexpr int gridDimY = 2;
		std::vector<rflx::MeshHandle> sphereGrid;
		sphereGrid.reserve(gridDimY * gridDimX);
		for (int y = 0; y < gridDimY; ++y)
		{
			for (int x = 0; x < gridDimX; ++x)
			{
				std::vector<PixelValue> albPixels;
				albPixels.resize(1, {255,255,255,255});
				rflx::ImageHandle albedo = rflx::CreateImage(std::move(albPixels));

				std::vector<PixelValue> matPixels;
				matPixels.resize(1, {uint8_t(float(x) / float(gridDimX) * 256.f),uint8_t((y > 0) * 255),0,0});
				rflx::ImageHandle material = rflx::CreateImage(std::move(matPixels));

				rflx::MeshHandle sphereHandle = rflx::CreateMesh("Assets/sphere.fbx", {albedo, material});

				sphereGrid.emplace_back(sphereHandle);
			}
		}
		
		std::vector<rflx::CubeHandle> skyboxes;
		skyboxes.emplace_back(rflx::CreateImageCube("Assets/Cube Maps/stor_forsen.dds"));
		skyboxes.emplace_back(rflx::CreateImageCube("Assets/Cube Maps/yokohama.dds"));

		rflx::ImageHandle test = rflx::CreateImage("Assets/Textures/element.tga", {3,3});
		
		ThreadFunc render = [&] (float dt, float tt, int fnr)
		{
			reflexA.BeginPush();
			float fps = 1.f / dt;
			auto tFPS = std::to_string(int(fps));
			tFPS.append(" fps");
			reflexA.PushRenderCommand(FontID(0),
				tFPS.c_str(),
				{ -.99, .89, 0 },
				.08f,
				{ 0,1,1,1 });
			for (int y = 0; y < gridDimY; ++y)
			{
				for (int x = 0; x < gridDimX; ++x)
				{
					Vec3f pos = { x,y, 0 };
					pos.x -= gridDimX / 2.25f;
					pos.y -= gridDimY / 2.25f;
					pos.x *= 2;
					pos.y *= -2;
					reflexA.PushRenderCommand(sphereGrid[y * gridDimX + x], pos, {1,1,1}/*, {0,1,0}, tt*/);
				}
			}
			reflexA.EndPush();

			reflexA.BeginFrame();
			reflexA.Submit();
			reflexA.EndFrame();
		};

		ThreadFunc logic = [&] (float dt, float tt, int fnr)
		{
			Sleep(1000/120);
			
			InputHandler::BeginFrame();
			reflexB.BeginPush();

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

			for (int skyboxIndex = 0; skyboxIndex < skyboxes.size(); ++skyboxIndex)
			{
				if (InputHandler::IsPressed('1' + skyboxIndex))
				{
					skyboxes[skyboxIndex].SetAsSkybox();
				}
			}

			reflexB.SetView({0, y,0}, {xRot, yRot}, distance);
			
			for (int y = 0; y < gridDimY; ++y)
			{
				for (int x = 0; x < gridDimX; ++x)
				{
					Vec3f pos = { x,y, 0 };
					pos.x -= gridDimX / 2.25f;
					pos.y -= gridDimY / 2.25f;
					pos.x *= 2;
					pos.y *= -2;
					//reflexB.PushRenderCommand(sphereGrid[y * gridDimX + x], pos, {1,1,1}/*, {0,1,0}, tt*/);
				}
			}

			reflexB.PushRenderCommand(boyHandle, {}, {1,1,1}, {0,1,0}, tt * 10);

			auto [mx, my] = InputHandler::GetMousePos();
			reflexB.SetScaleReference(test, { 1,-1 });
			reflexB.PushRenderCommand(test, uint32_t(tt) % 16, {0,0}, {1,1});

			float fps = 1.f / dt;
			auto tFPS = std::to_string(int(fps));
			tFPS.append(" fps");
			reflexB.PushRenderCommand(FontID(0),
				tFPS.c_str(),
				{ -.99, .99, 0 },
				.08f,
				{ 0,1,1,1 });

			reflexB.EndPush();
			InputHandler::EndFrame();
		};

		ThreadFunc funcs[]{render, logic};
		
		
		MultiApplication app
		(
			window,
			{render, logic}
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