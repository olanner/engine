
#pragma once

#include <concurrent_priority_queue.h>
#include <mutex>

#define INVALID_ID -1
#define BAD_ID(id) (int(id) < 0)

template<typename IDType>
class IDKeeper
{
public:
	IDKeeper( unsigned numIDSlots );

	IDType FetchFreeID();
	void ReturnID( const IDType& id );

private:
	concurrency::concurrent_priority_queue<IDType, std::greater<>>	myFreeIDs;
#ifdef _DEBUG
	std::mutex														myOccupiedTamperMutex;
	std::vector<IDType>												myOccupiedIDs;
#endif

};

template<typename IDType>
inline IDKeeper<IDType>::IDKeeper( unsigned numIDSlots )
{
	for ( int id = 0; id < numIDSlots; ++id )
	{
		myFreeIDs.push( IDType( id ) );
	}
}

template<typename IDType>
inline IDType IDKeeper<IDType>::FetchFreeID()
{
	IDType ret = IDType( INVALID_ID );
	if ( !myFreeIDs.try_pop( ret ) )
	{
		return IDType( INVALID_ID );
	}
#ifdef _DEBUG
	{
		std::scoped_lock<std::mutex> lock( myOccupiedTamperMutex );
		myOccupiedIDs.push_back( ret );
	}
#endif
	return ret;
}

template<typename IDType>
inline void IDKeeper<IDType>::ReturnID( const IDType& id )
{
#ifdef _DEBUG
	{
		std::scoped_lock<std::mutex> lock( myOccupiedTamperMutex );
		auto iter = std::find(myOccupiedIDs.begin(), myOccupiedIDs.end(), id);
		if (iter != myOccupiedIDs.end())
		{
			assert(false && "attempting to return unoccupied id");
		}
		else
		{
			myOccupiedIDs.erase(iter);
		}
	}
#endif
	myFreeIDs.push(id);
}
