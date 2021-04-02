#pragma once

struct WindowParams
{
	const wchar_t* title = L"Hypo Gloss";
	int width = 800;
	int height = 600;
	bool windowedFullscreenFlag = false;
	std::function<bool(HWND, UINT, WPARAM, LPARAM)> onWinProc = [](HWND, UINT, WPARAM, LPARAM)->bool{return true;};
};