/*
 * Prerequisites: ZMQ needs to be installed Program Files (x86)\ZeroMQ 4.0.4 (Installer available via http://miru.hk/archive/ZeroMQ-4.0.4~miru1.0-x86.exe)
 * Boost: http://sourceforge.net/projects/boost/files/boost-binaries/1.57.0/boost_1_57_0-msvc-9.0-32.exe/download
 */

#include "stdafx.h"

#include <string>
#include <vector>
#include <iostream>

#include "ZmqClient.h"

using namespace std;

int main(int argc, char* argv) {
	ZmqClient zmqClient;

	zmqClient.startReceiverThread();

	cout << "Welcome" << endl;
	std::string msg;
	do {
		cout << "Please enter a message to be sent: ";
		getline(std::cin, msg);

		if (msg.compare("quit") == 0) {
			break;
		}
		zmqClient.sendMessage(std::move(msg));
	} while (true);

	std::cout << "Shutting down" << std::endl;

	return 0;
}

