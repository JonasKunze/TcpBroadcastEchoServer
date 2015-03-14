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
#include <vector>

#include "SocketState.h"
#include "ThreadSafeProducerConsumerQueue.h"

#define WRITE_THREAD_NUM 2
#define READ_THREAD_NUM 2
class Server
{
public:
	Server(unsigned int portNumber, unsigned long receiveAddress, bool nodelay, std::vector<std::pair<std::string, unsigned int>> otherServerAddressesAndPorts);
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

	const std::vector<std::pair<std::string, unsigned int>> otherServerAddressesAndPorts;

	// Socket state used for connection establishements
	AcceptState mySocketState;

	// Overlapped object for connection purposes
	WSAOVERLAPPED mySocketOverlapped;

	// Stuff used for AcceptEx function
	LPFN_ACCEPTEX pfAcceptEx;
	GUID GuidAcceptEx;

	// all connected clients and servers
	typedef std::shared_ptr<SocketState> SocketState_ptr;
	std::set<SocketState_ptr> clients;

	// connected server
	SocketState_ptr connectedServer;

	// Signaling for slave server disconnection
	std::condition_variable slaveServerDisconnectedCondVar;
	std::mutex slaveServerDisconnectedMutex;

	std::thread slaveConnectionThread;

	// mutex for clients (used for disconnections)
	std::mutex clientsMutex;

	// Queues for write jobs (broadcasts). One queue per thread
	ThreadsafeProducerConsumerQueue<std::function<void()>> writeJobs[WRITE_THREAD_NUM];
	int writeJobRoundRobin;
	
	// Qeueu for read jobs (incoming messages)
	ThreadsafeProducerConsumerQueue<SocketState_ptr> readJobs[READ_THREAD_NUM];

	void initWinsock();

	void createIoCompletionPort();
	 
	SOCKET createClientSocket();

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

	 void startAccepting();

	 void readThread(const int threadNum);

	 void writeThread(const int threadNum);

	 void connectSlaveServer();

};

