#pragma once
#include <chrono>


class Timer
{
public:
	//Timer(const Timer &aTimer) = delete;
	//Timer& operator=(const Timer &aTimer) = delete;

	Timer();

	void Start();
	void Tick();
	
	float GetDeltaTime();
	float GetTotalTime();

private:
	
	void SetStartingTime(const std::chrono::high_resolution_clock::time_point &timePoint);

	void SetStartOfFrameTime( const std::chrono::high_resolution_clock::time_point &now );
	void SetDeltaTime( const std::chrono::high_resolution_clock::time_point &now );
	void SetTotalTime( const std::chrono::high_resolution_clock::time_point &now );

	std::chrono::high_resolution_clock::time_point myStartingTime;
	std::chrono::high_resolution_clock::time_point myStartOfFrameTime;
	float myDeltaTime;
	float myTotalTime;
	bool myHasStarted;

};
