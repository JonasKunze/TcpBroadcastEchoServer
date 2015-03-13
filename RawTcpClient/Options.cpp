#include "Options.h"

#include <iostream>

using namespace std;

std::vector<std::pair<std::string, unsigned int>> Options::servers;
bool Options::nodelay = false;

void Options::initialize(int argc, char *argv[]){
	for (int i = 1; i < argc; i++){
		if (strcmp (argv[i],"--nodelay") == 0) {
			cout << "Switching off Nagle's algorithm" << endl;
			nodelay = true;
			continue;
		}

		char address[256];
		long int port;
		
		if (sscanf_s(argv[i], "%[^:]:%d", address, sizeof(address), &port) != 2) {
			cerr << "Bad input format! Please use something like 'localhost:1234"
				<< endl;
			continue;
		}

		servers.push_back(std::make_pair(std::move(std::string(address)), port));
	}

	if (servers.empty()) {
		cout << "You have to provide at least one server address and port!" << endl;
		cout << "Please try using something similar to " << argv[0] << " localhost:1234" << endl;
		exit(1);
	}
	if (!nodelay){
		cout << "Use --nodelay to switch off nagle's algorithm" << endl;
	}
}
