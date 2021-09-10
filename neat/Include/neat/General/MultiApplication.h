
#pragma once
#include <thread>
#include <vector>

using  ThreadFunc = std::function<void(float, float, int)>;

class MultiApplication
{
public:
			MultiApplication(
				class Window& window, 
				std::vector<ThreadFunc> tickFunctions);
			~MultiApplication();

	WPARAM	Loop();
	void	Shutdown();

private:
	std::vector<std::thread*>	myThreads;
	std::vector<bool>			myIsRunnings;

	class Window&				myWindow;

};

