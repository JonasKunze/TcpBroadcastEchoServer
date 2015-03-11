// EchoServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "ZmqServer.h"

using namespace std;



int main(void) {
	ZmqServer server;
	server.startServer();
	return 0;
}
