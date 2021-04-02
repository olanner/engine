
#pragma once

#define VK_FALLTHROUGH(result) if(result) assert(false);
#define VK_ASSERT(result) assert(!result)

#define SAFE_DELETE(ptr) delete ptr; ptr = nullptr
#define SAFE_DELETE_ARRAY(ptr) delete[] ptr; ptr = nullptr

#define kB(num) num * 1024
//#define nodiscard [[nodiscard]]

#define NUM_MIPS( imgRes ) 1 + std::log2( imgRes )

