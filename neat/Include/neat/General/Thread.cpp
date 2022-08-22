#include "pch.h"
#include "Thread.h"

#include <utility>

std::atomic_int neat::Thread::ourThreadCount = 0;

neat::Thread::Thread()
	: myThreadID(ThreadID(ourThreadCount.fetch_add(1)))
	, myStartSignal(std::make_shared<std::binary_semaphore>(1))
	, myNextSignal(std::make_shared<std::binary_semaphore>(0))
{
}

neat::Thread::Thread(
	std::shared_ptr<std::binary_semaphore> startSignal)
	: myThreadID(ThreadID(ourThreadCount.fetch_add(1)))
	, myStartSignal(std::move(startSignal))
	, myNextSignal(std::make_shared<std::binary_semaphore>(0))
{
}

neat::Thread::~Thread()
{
	//assert(!myIsRunning && "thread not made to join before end of lifetime");
	while (!Join());
}

neat::Thread::Thread(Thread&& thread) noexcept
	: myStartFunc(std::move(thread.myStartFunc))
	, myMainFunc(std::move(thread.myMainFunc))
	, myIsRunning(thread.myIsRunning)
	, myIsJoined(thread.myIsJoined)
	, myThread(std::move(thread.myThread))
	, myThreadID(thread.myThreadID)
	, myStartSignal(std::move(thread.myStartSignal))
	, myNextSignal(std::move(thread.myNextSignal))
{
	thread.myThread.detach();
}

neat::Thread&
neat::Thread::operator=(
	Thread&& thread) noexcept
{
	myStartFunc = std::move(thread.myStartFunc);
	myMainFunc = std::move(thread.myMainFunc);
	myIsRunning = thread.myIsRunning;
	myIsJoined = thread.myIsJoined;
	myThread = std::move(thread.myThread);
	myThreadID = thread.myThreadID;
	myStartSignal = std::move(thread.myStartSignal);
	myNextSignal = std::move(thread.myNextSignal);

	return *this;
}

std::shared_ptr<std::binary_semaphore>
neat::Thread::GetNextSignal() const
{
	return myNextSignal;
}

void
neat::Thread::Start()
{
	assert(myStartFunc && myMainFunc && "both start and main functions need to be callable!");
	
	myIsRunning = true;
	myThread = std::thread([this]() -> void
	{
		myStartSignal->acquire();
		myStartFunc();
		myNextSignal->release();
		myMainFunc();
		myStartSignal->release();
		//myNextSignal->acquire();
	});
}

bool
neat::Thread::IsRunning() const
{
	return myIsRunning;
}

bool
neat::Thread::Join()
{
	myIsRunning = false;
	if (myThread.joinable())
	{
		myThread.join();
		myIsJoined = true;
		return myIsJoined;
	}
	return myIsJoined;
}

neat::ThreadID
neat::Thread::GetThreadID() const
{
	return myThreadID;
}
