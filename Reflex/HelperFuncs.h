
#pragma once
#include <rapidjson/document.h>

inline rapidjson::Document
OpenJsonDoc(const char* path)
{
	rapidjson::Document doc;

	std::ifstream txt;
	txt.open(path);
	if (!txt.good())
	{
		doc.Parse("{}");
		return doc;
	}
	std::string str((std::istreambuf_iterator<char>(txt)),
					 std::istreambuf_iterator<char>());
	doc.Parse(str.c_str());
	return doc;
}


template<typename ... Args>
void
LOG(Args&& ... someArgs)
{
	std::ofstream out;
	out.open("log.txt", std::ios::app);
	((std::cout << someArgs << " "), ...);
	std::cout << '\n';
	((out << someArgs << " "), ...);
	out << '\n';

	out.close();
}