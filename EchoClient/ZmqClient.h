#pragma once

#include <zmq.h>
#include <string>
#include <iostream>

namespace std{
	class thread;
}

class ZmqClient {
public:
	ZmqClient();
	virtual ~ZmqClient();

	void inline sendMessage(std::string&& msg) {
		std::cout << "Sending " << msg << std::endl;
		zmq_send(sendSocket, msg.data(), msg.size(), 0);
	}

	void startReceiverThread();

private:
	std::thread* receiverThread_;
	void* zmqContext;
	void* sendSocket;
	void* receiveSocket;

	void receiverThread();
};

