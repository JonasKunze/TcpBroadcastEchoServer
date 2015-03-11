#pragma once

#include <windows.h>

class Utils
{
public:
	static long long getCurrentMillis(){
		SYSTEMTIME st;
		GetSystemTime(&st);
		return (long long)st.wSecond * 1000 + st.wMilliseconds;
	}
};

