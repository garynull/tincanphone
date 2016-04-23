/*
	(C) 2016 Gary Sinitsin. See LICENSE.txt (MIT license).
*/
#pragma once

// Integer types with specified bit widths
#ifdef _MSC_VER
namespace tincan {
	typedef unsigned char  byte;
	typedef unsigned char  uint8;
	typedef signed char    int8;
	typedef unsigned short uint16;
	typedef signed short   int16;
	typedef unsigned int   uint32;
	typedef signed int     int32;
}
#else
#include <stdint.h>
namespace tincan {
	typedef uint8_t        byte;
	typedef uint8_t        uint8;
	typedef int8_t         int8;
	typedef uint16_t       uint16;
	typedef int16_t        int16;
	typedef uint32_t       uint32;
	typedef int32_t        int32;
}
#endif

// Common std headers
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

// Shortcuts
namespace tincan {
	typedef unsigned char  uchar;
	typedef unsigned short ushort;
	typedef unsigned int   uint;
	typedef unsigned long  ulong;

	using std::string;
	using std::vector;
	using std::endl;

	template <typename T>
	string toString(const T& x)
	{
		std::ostringstream stream;
		stream << x;
		return stream.str();
	}
}
