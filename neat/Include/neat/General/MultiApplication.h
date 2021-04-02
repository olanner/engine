
#pragma once
#include <thread>
#include <vector>

using  TickFunc = std::function<void( float, float, int )>;

class MultiApplication
{
public:
	MultiApplication( class Window& window,
					  TickFunc* tickFuncs,
					  uint32_t  numTickFuncs );
	~MultiApplication();

	WPARAM Loop();
	void Shutdown();

private:
	std::vector<std::thread*> myThreads;
	std::vector<bool> myIsRunnings;

	class Window& myWindow;

};

