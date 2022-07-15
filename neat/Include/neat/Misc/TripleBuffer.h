#pragma once
#include "neat/Containers/static_vector.h"

namespace neat
{
	template<typename T, unsigned MaxBuffSize>
	class TripleBuffer
	{
	public:
		const static_vector<T, MaxBuffSize>& ReadNext();
		void WriteNext();
		void Push(T&& anItem);
		void Push(const T& anItem);

	private:
		std::array<static_vector<T, MaxBuffSize>, 3> myBuffers;
		unsigned myReadIndex = 0;
		unsigned myWriteIndex = 1;
		unsigned myFreeIndex = 2;
		bool myIsNextAvailable = true;
	};
	
	template<typename T, unsigned MaxBuffSize>
	inline const static_vector<T, MaxBuffSize>& TripleBuffer<T, MaxBuffSize>::ReadNext()
	{
		if (myIsNextAvailable)
		{
			std::swap(myReadIndex, myFreeIndex);
			myIsNextAvailable = false;
		}
		return myBuffers[myReadIndex];
	}

	template<typename T, unsigned MaxBuffSize>
	inline void TripleBuffer<T, MaxBuffSize>::WriteNext()
	{
		std::swap(myWriteIndex, myFreeIndex);
		myBuffers[myWriteIndex].clear();
		myIsNextAvailable = true;
	}

	template<typename T, unsigned MaxBuffSize>
	inline void TripleBuffer<T, MaxBuffSize>::Push(T&& anItem)
	{
		myBuffers[myWriteIndex].emplace_back(std::forward<T>(anItem));
	}

	template<typename T, unsigned MaxBuffSize>
	inline void TripleBuffer<T, MaxBuffSize>::Push(const T& anItem)
	{
		Push(T(anItem));
	}

}

