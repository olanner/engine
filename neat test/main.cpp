// neat Test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <vector>

#include "neat/Containers/static_vector.h"
#include "neat/Image/DDSReader.h"

#ifdef _DEBUG
#pragma comment(lib, "neat_Debugx64.lib")
#else
#pragma comment(lib, "neat_Releasex64.lib")
#endif

#include "neat/Image/ImageReader.h"

int main()
{
	neat::Image image = neat::ReadImage("test.tga");

	int val = 0;
}

