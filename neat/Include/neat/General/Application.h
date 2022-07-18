
#pragma once

namespace neat {
	class Application
	{
	public:
		Application(class Window& window, const std::function<void(float, float, int)>&& tickFunction);
		~Application();

		WPARAM Loop();
		HRESULT Init();

		void Shutdown();

	private:
		std::function<void(float/*DeltaTime*/, float/*TotalTime*/, int/*FrameNR*/)> myTickFunction;

		class Window& myWindow;

	};
}

