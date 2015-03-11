#include "ZmqClient.h"
#include <assert.h>
#include <iostream>
#include <thread>

#define MTU 1500

using namespace std;

ZmqClient::ZmqClient() :
zmqContext(zmq_ctx_new()), sendSocket(zmq_socket(zmqContext, ZMQ_PUSH)), receiveSocket(zmq_socket(zmqContext, ZMQ_SUB)) {
	int rc = zmq_connect(sendSocket, "tcp://localhost:5555");
	assert(rc == 0);

	rc = zmq_connect(receiveSocket, "tcp://localhost:5556");
	assert(rc == 0);

	zmq_setsockopt(receiveSocket, ZMQ_SUBSCRIBE, "", 0);
}

ZmqClient::~ZmqClient() {
	zmq_close(sendSocket);
	zmq_close(receiveSocket);
	zmq_ctx_destroy(zmqContext);
}

void ZmqClient::startReceiverThread() {
	receiverThread_ = new std::thread(&ZmqClient::receiverThread, this);
}

void ZmqClient::receiverThread() {
	while (true){
		char buffer[MTU];
		// Receive the next message
		int len = zmq_recv(receiveSocket, buffer, MTU, 0);

		if (len == -1) {
			break;
		}

		if (len > MTU - 1) {
			cerr << "Received too long message -> truncating" << endl;
			len = MTU - 1;
		}
		buffer[len] = '\0';

		cout << "Received " << buffer << endl;
	}
}