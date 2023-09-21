#pragma once
#include "neat/Containers/static_vector.h"

namespace neat
{
	enum class TripleBufferFlags : int
	{
		HasWrittenGuarantee	= 0x01,
		HasReadGuarantee	= 0x02,
	};
	constexpr TripleBufferFlags operator|(TripleBufferFlags lhs, TripleBufferFlags rhs)
	{
		return static_cast<TripleBufferFlags>(
			static_cast<std::underlying_type_t<TripleBufferFlags>>(lhs) |
			static_cast<std::underlying_type_t<TripleBufferFlags>>(rhs));
	}
	constexpr bool operator&(TripleBufferFlags lhs, TripleBufferFlags rhs)
	{
		return
			static_cast<std::underlying_type_t<TripleBufferFlags>>(lhs) &
			static_cast<std::underlying_type_t<TripleBufferFlags>>(rhs);
	}

	template<typename T, unsigned MaxBuffSize>
	class TripleBuffer
	{
	public:
		void	AddFlags(TripleBufferFlags flags);
		void	BeginNextRead();
		void	BeginNextWrite();
		const static_vector<T, MaxBuffSize>&
				Read();
		static_vector<T, MaxBuffSize>&
				Write();

	private:
		TripleBufferFlags		myFlags = TripleBufferFlags(0);
		std::array<static_vector<T, MaxBuffSize>, 3>
								myBuffers;
		unsigned				myReadIndex = 0;
		unsigned				myWriteIndex = 1;
		unsigned				myFreeIndex = 2;
		bool					myHasWritten = true;
		bool					myHasRead = true;
		std::mutex				myMutex;
	};

	template<typename T, unsigned MaxBuffSize>
	inline void TripleBuffer<T, MaxBuffSize>::AddFlags(TripleBufferFlags flags)
	{
		myFlags = myFlags | flags;
	}

	template<typename T, unsigned MaxBuffSize>
	inline void TripleBuffer<T, MaxBuffSize>::BeginNextRead()
	{
		if (!myHasWritten
			&& myFlags & TripleBufferFlags::HasWrittenGuarantee)
		{
			return;
		}
		std::lock_guard lock(myMutex);
		std::swap(myReadIndex, myFreeIndex);
		myHasRead = true;
		myHasWritten = false;
	}

	template<typename T, unsigned MaxBuffSize>
	inline void TripleBuffer<T, MaxBuffSize>::BeginNextWrite()
	{
		if (myHasRead
			&& myFlags & TripleBufferFlags::HasReadGuarantee)
		{
			return;
		}
		std::lock_guard lock(myMutex);
		std::swap(myWriteIndex, myFreeIndex);
		myBuffers[myWriteIndex].clear();
		myHasRead = false;
		myHasWritten = true;
	}

	template<typename T, unsigned MaxBuffSize>
	inline const static_vector<T, MaxBuffSize>& TripleBuffer<T, MaxBuffSize>::Read()
	{
		return myBuffers[myReadIndex];
	}

	template<typename T, unsigned MaxBuffSize>
	inline static_vector<T, MaxBuffSize>& TripleBuffer<T, MaxBuffSize>::Write()
	{
		return myBuffers[myWriteIndex];
	}

}

