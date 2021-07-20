#include "pch.h"

#define _DEVELOPMENT

#include <string>

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
	{
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
		Reflex reflex(info, _bstr_t(lpCmdLine));
		if (!reflex.IsGood())
		{
			MessageBox(window.GetWindowHandle(), L"reflex failed to start", L"error", NULL);
			return 0;
		}


		MeshHandle plane = rflx::CreateMesh("Assets/Sascha/plane.obj", {});
		MeshHandle plane1 = rflx::CreateMesh("Assets/plane.obj", {});
		//MeshHandle sphereRows = rflx::LoadMesh( "Assets/sphere_rows.fbx", {albedo, material});

		MeshHandle dragon = rflx::CreateMesh("Assets/Sascha/dragon.fbx", {rflx::CreateImage({ {255,0,0,255} }),rflx::CreateImage({ {255,0,0,255} })});
		MeshHandle venus = rflx::CreateMesh("Assets/Sascha/venus.fbx", {});
		MeshHandle knot = rflx::CreateMesh("Assets/Sascha/torusknot.fbx", {});


		constexpr int gridDimX = 7;
		constexpr int gridDimY = 2;
		std::vector<MeshHandle> sphereGrid;
		sphereGrid.reserve(gridDimY * gridDimX);
		for (int y = 0; y < gridDimY; ++y)
		{
			for (int x = 0; x < gridDimX; ++x)
			{
				std::vector<PixelValue> albPixels;
				albPixels.resize(1, {255,255,255,255});
				ImageHandle albedo = rflx::CreateImage(std::move(albPixels));

				std::vector<PixelValue> matPixels;
				matPixels.resize(1, {uint8_t(float(x) / float(gridDimX) * 256.f),uint8_t((y > 0) * 255),0,0});
				ImageHandle material = rflx::CreateImage(std::move(matPixels));

				MeshHandle sphereHandle = rflx::CreateMesh("Assets/sphere.fbx", {albedo, material});

				sphereGrid.emplace_back(sphereHandle);
			}
		}

		std::vector<CubeHandle> skyboxes;
		skyboxes.emplace_back(rflx::CreateImageCube("Assets/Cube Maps/stor_forsen.dds"));
		skyboxes.emplace_back(rflx::CreateImageCube("Assets/Cube Maps/yokohama.dds"));

		TickFunc render = [&] (float dt, float tt, int fnr)
		{
			float fps = 1.f / dt;
			auto tFPS = std::to_string(int(fps));
			tFPS.append(" fps");
			reflex.BeginFrame();
			rflx::PushRenderCommand(FontID(0),
									tFPS.c_str(),
									{-.99, .99, 0},
									.08f,
									{0,1,1,1});
			reflex.Submit();
			reflex.EndFrame();
		};

		TickFunc logic = [&] (float dt, float tt, int fnr)
		{
			InputHandler::BeginFrame();

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
				dx = dx < 0 ? -1 : dx > 0 ? 1 : 0;
				dy = dy < 0 ? -1 : dy > 0 ? 1 : 0;
				yRot += dx * distance * dt;
				xRot += -dy * distance * dt;
			}
			distance += InputHandler::IsHeld(VK_UP) * 128.f * dt;
			distance -= InputHandler::IsHeld(VK_DOWN) * 128.f * dt;

			if (InputHandler::IsPressed(VK_F1))
			{
				static bool flag = true;
				rflx::SetUseRayTracing(flag = !flag);
			}

			for (int skyboxIndex = 0; skyboxIndex < skyboxes.size(); ++skyboxIndex)
			{
				if (InputHandler::IsPressed('1' + skyboxIndex))
				{
					skyboxes[skyboxIndex].SetAsSkybox();
				}
			}

			rflx::SetView({0, y,0}, {xRot, yRot}, distance);

			rflx::BeginPush();

			//rflx::PushRenderCommand( plane1, {0,0,0}, {1,1,1});
			//rflx::PushRenderCommand( dragon, {0,0,0}, { .33f,.33f,.33f } );
			//rflx::PushRenderCommand( sphere, {-3,1,0}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {3, 1,0}, { .66f,.66f,.66f } );
			////rflx::PushRenderCommand( sphere, {0, 1,3}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {0, 1,-3}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {-3,3,0}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {3, 3,0}, { .66f,.66f,.66f } );
			////rflx::PushRenderCommand( sphere, {0, 3,3}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {0, 3,-3}, { .66f,.66f,.66f } );
			//
			//rflx::PushRenderCommand( sphere, {0, 4,0}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {-2, 2,-2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {2, 2,-2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {-3, 2,2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {3, 2,2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {-2, 4,-2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {2, 4,-2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {-2, 4,2}, { .66f,.66f,.66f } );
			//rflx::PushRenderCommand( sphere, {2, 4,2}, { .66f,.66f,.66f } );


			for (int y = 0; y < gridDimY; ++y)
			{
				for (int x = 0; x < gridDimX; ++x)
				{
					Vec3f pos = {x,y, 0};
					pos.x -= gridDimX / 2.25f;
					pos.y -= gridDimY / 2.25f;
					pos.x *= 2;
					pos.y *= -2;
					rflx::PushRenderCommand(sphereGrid[y * gridDimX + x], pos);
				}
			}

			rflx::PushRenderCommand( plane, {} );
			rflx::PushRenderCommand( dragon, {3,0,3}, { .33f,.33f,.33f }, {0,1,0}, 0 );
			rflx::PushRenderCommand( venus, {-3,0,-3}, { .33f,.33f,.33f }, {0,1,0}, 3.14 + 3.14 / 4);
			rflx::PushRenderCommand( knot, {0,3,0}, { .075f,.075f,.075f }, {0,1,0}, 0 );

			//for ( int i = 0; i < 16; ++i )
			//{
			//	const float x = cosf( float( i ) / 16.f * 6.18f + tt ) * 8.f;
			//	const float z = sinf( float( i ) / 16.f * 6.18f + tt ) * 8.f;
			//	rflx::PushRenderCommand( sphereGrid[90],
			//							 { x, 2.f,  z },
			//							 { .66f,.66f,.66f }
			//	);
			//	//rflx::PushRenderCommand( sphereHandle, 
			//	//						 { i % 8 * 2 - 7, 0,i / 8 * 2 - 7 }, 
			//	//						 {.5f, .5f, .5f}, 
			//	//						 {0,0,1},//glm::normalize( Vec3f( ( cosf( tt ) ), ( sinf( tt ) ), -.5f ) ),
			//	//						 0);//tt *.25f + i );
			//}



			rflx::EndPush();

			InputHandler::EndFrame();
		};

		TickFunc funcs[]{render, logic};

		Application app
		(
			window,
			[&] (float a, float b, float c)
		{
			render(a, b, c); logic(a, b, c);
		}
		);

		/*MultiApplication app
		(
			window,
			funcs,
			ARRAYSIZE( funcs )
		);*/

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