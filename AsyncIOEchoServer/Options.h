#pragma once

#include <set>
#include <string>

class Options
{
public:
	static void initialize(int argc, char *argv[]);
	static unsigned int portNumber;
	static std::set<std::pair<std::string, unsigned int>> servers;
	static bool nodelay;
	static bool noEcho;
};

