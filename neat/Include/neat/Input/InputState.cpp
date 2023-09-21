#include "pch.h"
#include "InputState.h"

InputState::InputState()
	: myKeyLastHeld(128)
	, myKeyLastReleased(128)
	, myKeyLastPressed(128)
{
}

void
InputState::SetWindowSize(
	float width, 
	float height)
{
	myWindowWidth = width;
	myWindowHeight = height;
}

void
InputState::SetTimeReference(
	long timePoint)
{
	myTimeReference = timePoint;
}

void
InputState::SetLastHeld(
	VirtualKey	key, 
	long		timePointWhen)
{
	myKeyLastHeld[key] = timePointWhen;
}

void
InputState::SetLastReleased(
	VirtualKey	key,
	long		timePointWhen)
{
	myKeyLastReleased[key] = timePointWhen;
}

void
InputState::SetLastPressed(
	VirtualKey	key,
	long		timePointWhen)
{
	myKeyLastPressed[key] = timePointWhen;
}

bool
InputState::IsHeld(
	VirtualKey	key, 
	long		millisecondBias)
{
	return myTimeReference - myKeyLastHeld[key] < millisecondBias;
}

bool
InputState::IsReleased(
	VirtualKey	key, 
	long		millisecondBias)
{
	return myTimeReference - myKeyLastReleased[key] < millisecondBias;
}

bool
InputState::IsPressed(
	VirtualKey	key, 
	long		millisecondBias)
{
	return myTimeReference - myKeyLastPressed[key] < millisecondBias;
}

void
InputState::AddMouseWheelDelta(int wheelDelta)
{
	myMouseWheelDelta += wheelDelta;
}

void
InputState::ResetMouseWheelDelta()
{
	myMouseWheelDelta = 0;
}

int
InputState::GetMouseWheelDelta() const
{
	return myMouseWheelDelta;
}

void
InputState::SetMousePosition(
	int x, 
	int y)
{
	myMousePositionDeltaX += myMousePositionDeltaX - x;
	myMousePositionDeltaY += myMousePositionDeltaX - y;
	myMousePositionX = x;
	myMousePositionY = y;
}

std::pair<int, int> InputState::GetMousePosition() const
{
	return {myMousePositionDeltaX, myMousePositionDeltaY};
}

std::pair<float, float>
InputState::GetNormalizedMousePosition() const
{
	return {float(myMousePositionX) / myWindowWidth, float(myMousePositionY) / myWindowHeight };
}
