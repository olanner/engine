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


void TEST_STATIC_VECTOR()
{
	constexpr int size = 128;
	neat::static_vector<int, size> vector;

	std::vector<int> vec;

	for ( int i = 0; i < size + 8; ++i )
	{
		COUT_LINE( ( ( vector.emplace_back( int( i ) ) ) ? "true" : "false" ) );
	}

	for ( auto& obj : vector.reverse_iterate() )
	{
		COUT_LINE( obj );
		vector.erase(&obj);
	}
	system( "pause" );
}

void TestDDSReader()
{
	ReadDDS("texture.dds");
}

int main()
{
	TestDDSReader();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
