#include "ZmqServer.h"
#include <assert.h>
#include <iostream>

#define MTU 1500

using namespace std;

ZmqServer::ZmqServer() :zmqContext(zmq_ctx_new()), receiveSocket(zmq_socket(zmqContext, ZMQ_PULL)), sendSocket(zmq_socket(zmqContext, ZMQ_PUB)) {
	int rc = zmq_bind(receiveSocket, "tcp://*:5555");
	assert(rc == 0);

	rc = zmq_bind(sendSocket, "tcp://*:5556");
	assert(rc == 0);
}


ZmqServer::~ZmqServer() {
	zmq_close(receiveSocket);
	zmq_close(sendSocket);
	zmq_ctx_destroy(zmqContext);
}

void ZmqServer::startServer() {
	while (true) {
		char buffer[MTU];
		// Receive the next message
		int len = zmq_recv(receiveSocket, buffer, MTU, 0);

		if (len == -1){
			break;
		}

		if (len > MTU - 1) {
			cerr << "Received too long message -> truncating" << endl;
			len = MTU - 1;
		}
		buffer[len] = '\0';

		cout << "Received " << buffer << endl;

		// Send message to all connected clients
		zmq_send(sendSocket, buffer, len, 0);

		cout << "Sent " << buffer << endl;
	}
}