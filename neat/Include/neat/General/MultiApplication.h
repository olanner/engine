
#pragma once
#include <vector>
#include "Thread.h"

namespace neat {
	class MultiApplication
	{
	public:
												MultiApplication(
													class Window& window,
													std::vector<std::unique_ptr<Thread>>&& tickFunctions);
												~MultiApplication();

		WPARAM									Loop();
		void									Shutdown();

	private:
		std::vector<std::unique_ptr<Thread>>	myThreads;
		class Window&							myWindow;

	};
}

