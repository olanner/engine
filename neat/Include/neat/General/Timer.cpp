
#include "pch.h"


#include "Timer.h"

Timer::Timer() :
	myDeltaTime(0.f),
	myTotalTime(),
	myHasStarted()
{
}

void Timer::Start()
{
	std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
	SetStartingTime( now );
	SetStartOfFrameTime( now );
	myHasStarted = true;
}

void Timer::Tick()
{
	assert( myHasStarted );
	std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
	SetDeltaTime( now );
	SetStartOfFrameTime( now );
	SetTotalTime( now );
}

float Timer::GetDeltaTime()
{
	return myDeltaTime;
}

float Timer::GetTotalTime()
{
	return myTotalTime;
}

void Timer::SetStartingTime( const std::chrono::high_resolution_clock::time_point& timePoint )
{
	myStartingTime = timePoint;

}

void Timer::SetStartOfFrameTime( const std::chrono::high_resolution_clock::time_point& now )
{
	myStartOfFrameTime = now;
}

void Timer::SetDeltaTime( const std::chrono::high_resolution_clock::time_point& now )
{
	using s = std::chrono::duration<float, std::ratio<1,1>>;
	myDeltaTime = std::chrono::duration_cast<s>( now - myStartOfFrameTime ).count();
}

void Timer::SetTotalTime( const std::chrono::high_resolution_clock::time_point& now )
{
	using s = std::chrono::duration<float, std::ratio<1,1>>;
	myTotalTime = std::chrono::duration_cast<s>( now - myStartingTime ).count();
}