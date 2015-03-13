#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <set>
#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>
#include <memory>

#include "SocketState.h"
#include "ThreadSafeProducerConsumerQueue.h"

class Server
{
public:
	Server(unsigned int portNumber, unsigned long receiveAddress, bool nodelay);
	virtual ~Server();

	void run();

private:
	// the completion port
	HANDLE completionPort;

	// the socket for listening to new connections
	SOCKET mySocket;

	// my Socket port Number
	const unsigned int portNumber;

	// receiver address (defining which network device should be used).
	const unsigned long receiveAddress;

	// receiver address (defining which network device should be used).
	const unsigned long nodelay;

	// Socket state used for connection establishements
	AcceptState mySocketState;

	// Overlapped object for connection purposes
	WSAOVERLAPPED mySocketOverlapped;

	// Stuff used for AcceptEx function
	LPFN_ACCEPTEX pfAcceptEx;
	GUID GuidAcceptEx;

	// all connected clients
	typedef std::shared_ptr<SocketState> SocketState_ptr;
	std::set<SocketState_ptr> clients;

	// mutex for clients (used for disconnections)
	std::mutex clientsMutex;

	// Queue for write jobs (broadcasts)
	ThreadsafeProducerConsumerQueue<SocketState_ptr> writeJobs;

	// Qeueu for read jobs (incoming messages)
	ThreadsafeProducerConsumerQueue<SocketState_ptr> readJobs;

	void initWinsock();

	void createIoCompletionPort();
	 
	void createSocket();

	void asyncRead(SocketState_ptr socketState);

	void asyncBroadcast(SocketState_ptr socketState);

	void onSendComplete(BOOL resultOk, DWORD length,
		SocketState_ptr socketState);


	 void onAcceptComplete(BOOL resultOk, DWORD length,
		 AcceptState* socketState);
	 SOCKET createAcceptingSocket();
	 
	 void closeConnection(SocketState_ptr socketState);

	 BOOL getCompletionStatus(DWORD* length, SocketState_ptr* socketState,
		 WSAOVERLAPPED** ovl_res);
	 void loadAcceptEx();
	 SocketState_ptr createNewSocketState();

	 void onReadComplete(BOOL resultOk, DWORD length,
		 SocketState_ptr socketState);

	 void startAccepting();

	 void readThread();

	 void writeThread();

};

