#pragma once

#include <zmq.h>

class ZmqServer
{
public:
	ZmqServer();
	virtual ~ZmqServer();

	void startServer();

private:
	void* zmqContext;
	void* receiveSocket;
	void* sendSocket;
};

