#include "Options.h"

#include <iostream>

using namespace std;

unsigned int Options::portNumber=1234;
std::vector<std::pair<std::string, unsigned int>> Options::servers;
bool Options::nodelay = false;

/*
 * Retrieves all options from the given arguments
 */
void Options::initialize(int argc, char *argv[]) {
	if (argc < 2) {
		cerr << "No port number prvided => Using " << portNumber << endl;
		cout << "To provide a port number and a list of other servers please use " << argv[0] << " <portNum> <server1>:<port1> <server2>:<port2>..." << endl;
	}
	else {
		try {
			portNumber = std::stoi(argv[1]);
		}
		catch (invalid_argument &){
			cerr << "The first parameter must be an integer (port number)" << endl;
			exit(1);
		}
	}


	for (int i = 2; i < argc; i++){
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

	if (!nodelay){
		cout << "Use --nodelay to switch off nagle's algorithm" << endl;
	}

}
