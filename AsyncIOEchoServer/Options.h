#pragma once

#include <vector>
#include <string>

class Options
{
public:
	static void initialize(int argc, char *argv[]);
	static unsigned int portNumber;
	static std::vector<std::pair<std::string, unsigned int>> servers;
};

