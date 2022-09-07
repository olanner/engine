#pragma once

#include "neat/Misc/TripleBuffer.h"
#include "neat/General/Thread.h"

template<typename WorkType, int WorkPerSchedule, int WorkMax>
class WorkScheduler
{
public:
	typedef neat::static_vector<WorkType, WorkMax> AssembledWorkType;
	
	void BeginPush(neat::ThreadID threadID)
	{
		std::scoped_lock lock(mySwapMutex);
		mySchedules[int(threadID)].WriteNext();
	}
	void PushWork(neat::ThreadID threadID, const WorkType& cmd)
	{
		mySchedules[int(threadID)].Push(cmd);
	}
	void EndPush(neat::ThreadID threadID) {}
	[[nodiscard]] AssembledWorkType& AssembleScheduledWork()
	{
		std::scoped_lock lock(mySwapMutex);
			
		myAssembledWork.clear();
		for (const auto& scheduleID : myRegisteredSchedules)
		{
			for (const auto& cmd : mySchedules[int(scheduleID)].ReadNext())
			{
				myAssembledWork.emplace_back(cmd);
			}
		}
		return myAssembledWork;
	}
	void AddSchedule(neat::ThreadID threadID)
	{
		assert(int(threadID) < mySchedules.max_size() && int(threadID) >= 0 && "only 8 schedules allowed also no negative values >:(");
		std::scoped_lock lock(mySwapMutex);
		myRegisteredSchedules.emplace_back(threadID);
	}
	
private:
	std::mutex mySwapMutex;
	std::vector<neat::ThreadID> myRegisteredSchedules;
	std::array<neat::TripleBuffer<WorkType, WorkPerSchedule>, 8> mySchedules;
	AssembledWorkType myAssembledWork;
	
};


