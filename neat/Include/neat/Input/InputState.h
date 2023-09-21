#pragma once

using VirtualKey = int;
class InputState final
{
public:
			InputState();
	void	SetWindowSize(
				float width, 
				float height);

	void	SetTimeReference(long timePoint);

	void	SetLastHeld(
				VirtualKey	key,
				long		timePointWhen);
	void	SetLastReleased(
				VirtualKey	key,
				long		timePointWhen);
	void	SetLastPressed(
				VirtualKey	key,
				long		timePointWhen);

	bool	IsHeld(
				VirtualKey key, 
				long millisecondBias);
	bool	IsReleased(
				VirtualKey key,
				long millisecondBias);
	bool	IsPressed(
				VirtualKey key,
				long millisecondBias);

	void	AddMouseWheelDelta(int wheelDelta);
	void	ResetMouseWheelDelta();
	int		GetMouseWheelDelta() const;

	void	SetMousePosition(int x, int y);
	std::pair<int, int>
			GetMousePosition() const;
	std::pair<float, float>
			GetNormalizedMousePosition() const;

private:
	long myTimeReference = 0;

	std::unordered_map<VirtualKey, long> myKeyLastHeld;
	std::unordered_map<VirtualKey, long> myKeyLastReleased;
	std::unordered_map<VirtualKey, long> myKeyLastPressed;

	float myWindowWidth = 0.f;
	float myWindowHeight = 0.f;

	int myMouseWheelDelta = 0;
	int myMousePositionX = 0;
	int myMousePositionY = 0;
	int myMousePositionDeltaX = 0;
	int myMousePositionDeltaY = 0;
};