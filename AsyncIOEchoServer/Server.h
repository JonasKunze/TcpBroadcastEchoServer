#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <set>
#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>

#include "SocketState.h"
#include "ThreadSafeProducerConsumerQueue.h"

class Server
{
public:
	Server(unsigned long portNumber, unsigned long receiveAddress);
	virtual ~Server();

	void run();

private:
	// the completion port
	HANDLE completionPort;

	// the listening socket
	SOCKET mySocket;
	unsigned long portNumber;
	unsigned long receiveAddress;

	AcceptState mySocketState;
	WSAOVERLAPPED mySocketOverlapped;

	// utility: variables used to load the AcceptEx function
	LPFN_ACCEPTEX pfAcceptEx;
	GUID GuidAcceptEx;

	std::set<SocketState*> clients;
	std::mutex clientsMutex;

	ThreadsafeProducerConsumerQueue<SocketState*> writeJobs;
	ThreadsafeProducerConsumerQueue<SocketState*> readJobs;

	void initWinsock();

	void create_io_completion_port();
	 
	void createSocket();

	void prepareSocket();

	void start_reading(SocketState* socketState);

	void startBroadcasting(SocketState* socketState);

	void write_completed(BOOL resultOk, DWORD length,
		SocketState* socketState);


	 void accept_completed(BOOL resultOk, DWORD length,
		 AcceptState* socketState);
	 SOCKET create_accepting_socket();
	 void destroy_connection(SocketState* socketState);
	 BOOL get_completion_status(DWORD* length, SocketState** socketState,
		 WSAOVERLAPPED** ovl_res);
	 void load_accept_ex();
	 SocketState* new_socket_state();

	 void read_completed(BOOL resultOk, DWORD length,
		 SocketState* socketState);

	 void start_accepting();

	 void readThread();
	 void writeThread();

};

