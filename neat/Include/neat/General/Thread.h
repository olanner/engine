#pragma once

#include <thread>
#include <functional>
#include <semaphore>

// TODO: implement thread checker, constructor, join and start should be exclusive to one thread
namespace neat
{
	enum class ThreadID;
	inline constexpr int MaxThreadID = 16;
	class Thread
	{
		static std::atomic_int			ourThreadCount;
	protected:
												Thread();
												Thread(std::shared_ptr<std::binary_semaphore> startSignal);
	public:
		virtual									~Thread();
												Thread(const Thread&) = delete;
												Thread& operator=(const Thread&) = delete;

												Thread(Thread&& thread) noexcept;
												Thread& operator=(Thread&& thread) noexcept;
		
		void									Start();
		bool									IsRunning() const;
		bool									Join();
		ThreadID								GetThreadID() const;
		
		[[nodiscard]]
		std::shared_ptr<std::binary_semaphore>	GetNextSignal() const;
	
	protected:
		std::function<void()>					myStartFunc;
		std::function<void()>					myMainFunc;
	
	private:
		bool									myIsRunning = false;
		std::thread								myThread;
		ThreadID								myThreadID;
		
		std::shared_ptr<std::binary_semaphore>	myStartSignal;
		std::shared_ptr<std::binary_semaphore>	myNextSignal;
		
	};
}