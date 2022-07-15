#pragma once

#include "neat/Misc/TripleBuffer.h"

enum class ScheduleID;
template<typename WorkType, int WorkPerSchedule, int WorkMax>
class WorkScheduler
{
public:
	void BeginPush(ScheduleID scheduleID)
	{
		std::scoped_lock lock(mySwapMutex);
		mySchedules[int(scheduleID)].WriteNext();
	}
	void PushWork(ScheduleID scheduleID, const WorkType& cmd)
	{
		mySchedules[int(scheduleID)].Push(cmd);
	}
	void EndPush(ScheduleID scheduleID) {}
	[[nodiscard]] neat::static_vector<WorkType, WorkMax>& AssembleScheduledWork()
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
	void AddSchedule(ScheduleID scheduleID)
	{
		assert(int(scheduleID) < mySchedules.max_size() && int(scheduleID) >= 0 && "only 8 schedules allowed also no negative values >:(");
		std::scoped_lock lock(mySwapMutex);
		myRegisteredSchedules.emplace_back(scheduleID);
	}
	
private:
	std::mutex mySwapMutex;
	std::vector<ScheduleID> myRegisteredSchedules;
	std::array<neat::TripleBuffer<WorkType, WorkPerSchedule>, 8> mySchedules;
	neat::static_vector<WorkType, WorkMax> myAssembledWork;
	
};


