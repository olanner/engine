#pragma once

#define SAFE_DELETE(ptr) delete ptr; ptr = nullptr
#define SAFE_DELETE_ARRAY(ptr) delete[] ptr; ptr = nullptr

#define COUT_LINE(line) std::cout << line << '\n'