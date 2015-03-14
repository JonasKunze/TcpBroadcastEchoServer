#include "Options.h"

#include <iostream>

using namespace std;

unsigned int Options::portNumber=1234;
std::vector<std::pair<std::string, unsigned int>> Options::servers;
bool Options::nodelay = false;
bool Options::noEcho = false;

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

		if (strcmp(argv[i], "--noecho") == 0) {
			cout << "Switching off direct echo" << endl;
			noEcho = true;
			continue;
		}

		if (strcmp(argv[i], "--help") == 0) {
			cout << "Usage: "<< argv[0] << "<portNumber> <parameters> slaveServer1:port1 slaveServer2:port2..." << endl;
			cout << "Clients must connect to <portNumber>. Received messages are broadcasted to all connected clients and the first server available in the given slave server list." << endl;
			cout << "Following parameters are allowed:" << endl;
			cout << "\t--nodelay:\t switches off Nagle's algorithm" << endl;
			cout << "\t--noecho:\t Messages will not be sent back to the client the message comes from" << endl;
			exit(0);
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

	if (!servers.empty()){
		cout << "WARNING: you provided a list of slace servers without setting --noecho. This would lead messages to travel in infinite loops between the servers. Enforcing --noecho!" << endl;
		noEcho = true;
	}

}
