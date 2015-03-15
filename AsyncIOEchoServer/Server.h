#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <set>
#include <atomic>
#include <mutex>
#include <thread>
#include <map>
#include <iostream>
#include <memory>
#include <vector>

#include "SocketState.h"
#include "ThreadSafeProducerConsumerQueue.h"

typedef std::shared_ptr<SocketState> SocketState_ptr;
typedef std::pair<std::string, unsigned int> ServerAddress;

class Server
{
public:
	Server(unsigned int portNumber, unsigned long receiveAddress, bool nodelay, std::set<ServerAddress> otherServerAddressesAndPorts, 
		bool noecho, unsigned int sendThreadNum, unsigned int receiveTheadNum);
	virtual ~Server();

	void run();

private:
	const unsigned int receiveTheadNum;
	const unsigned int sendThreadNum;

	// the completion port
	HANDLE completionPort;

	// the socket for listening to new connections
	SOCKET clientAcceptorSocket;
	SOCKET serverAcceptorSocket;

	// my Socket port Number
	const unsigned int portNumber;

	// receiver address (defining which network device should be used).
	const unsigned long receiveAddress;

	// receiver address (defining which network device should be used).
	const unsigned long nodelay;

	// If set to true messages will not be sent back to the client the message comes from. 
	// This must be true if the server connects to other slave server to avoid infinite loops
	const bool noecho;

	std::map<SocketState_ptr, ServerAddress> connectedServerAddressesAndPorts;
	std::set<ServerAddress> disconnectedServerAddressesAndPorts;

	// Socket state used for connection establishements
	AcceptState clientAcceptor;
	AcceptState serverAcceptor;

	// Overlapped object for connection purposes
	WSAOVERLAPPED mySocketOverlapped;

	// Stuff used for AcceptEx function
	LPFN_ACCEPTEX pfAcceptEx;
	GUID GuidAcceptEx;

	// all connected clients and servers
	std::set<SocketState_ptr> clients;
	
	// Signaling for slave server disconnection
	std::condition_variable slaveServerDisconnectedCondVar;
	std::mutex slaveServerDisconnectedMutex;

	std::thread slaveConnectionThread;

	// mutex for clients (used for disconnections)
	std::mutex clientsMutex;

	// Queues for write jobs (broadcasts). One queue per thread
	ThreadsafeProducerConsumerQueue<std::function<void()>>* writeJobs;
	int writeJobRoundRobin;
	
	// Queue for read jobs (incoming messages)
	ThreadsafeProducerConsumerQueue<SocketState_ptr>* readJobs;

	void initWinsock();

	void createIoCompletionPort();
	 
	SOCKET createAcceptorSocket(unsigned int portNumber, AcceptState& acceptor);

	void asyncRead(SocketState_ptr socketState);

	void asyncBroadcast(SocketState_ptr socketState);

	void onSendComplete(BOOL resultOk, DWORD length,
		SocketState_ptr socketState);


	 void onAcceptComplete(BOOL resultOk, DWORD length,
		 AcceptState* socketState);
	 SOCKET createSocket();
	 
	 void closeConnection(SocketState_ptr socketState);

	 BOOL getCompletionStatus(DWORD* length, SocketState_ptr* socketState,
		 WSAOVERLAPPED** ovl_res);
	 void loadAcceptEx();
	 SocketState_ptr createNewSocketState();

	 void onReadComplete(BOOL resultOk, DWORD length,
		 SocketState_ptr socketState);

	 void startAccepting(AcceptState* socketState, SOCKET socket);

	 void readThread(const int threadNum);

	 void writeThread(const int threadNum);

	 void connectSlaveServer();

};

