#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <set>
#include <atomic>
#include <mutex>


enum Configs {
	MAX_BUF = 1024,
	SERVER_ADDRESS = INADDR_LOOPBACK,
	SERVICE_PORT = 1234
};

enum SocketOperations {
	OP_NONE,
	OP_ACCEPT,
	OP_READ,
	OP_WRITE,
	OP_BROADCAST
};

struct SocketState {
	char operation;
	SOCKET socket;
	DWORD length;
	char buf[MAX_BUF];
	std::atomic<int> activeOperations;
	bool markDisconnect;
	SocketState* sendState;
};

class Server
{
public:
	Server();
	virtual ~Server();

	void run();

private:
	// the completion port
	HANDLE completionPort;

	// the listening socket
	SOCKET mySocket;
	SocketState mySocketState;
	WSAOVERLAPPED mySocketOverlapped;

	// utility: variables used to load the AcceptEx function
	LPFN_ACCEPTEX pfAcceptEx;
	GUID GuidAcceptEx;

	std::set<SocketState*> clients;
	std::mutex clientsMutex;

	void initWinsock();

	void create_io_completion_port();
	 
	void createSocket();

	void prepareSocket();

	void start_reading(SocketState* socketState, WSAOVERLAPPED* ovl);

	void startBroadcasting(SocketState* socketState, WSAOVERLAPPED* ovl);

	void write_completed(BOOL resultOk, DWORD length,
		SocketState* socketState, WSAOVERLAPPED* ovl);


	 void accept_completed(BOOL resultOk, DWORD length,
		SocketState* socketState, WSAOVERLAPPED* ovl);
	 SOCKET create_accepting_socket();
	 void destroy_connection(SocketState* socketState, WSAOVERLAPPED* ovl);
	 BOOL get_completion_status(DWORD* length, SocketState** socketState,
		WSAOVERLAPPED** ovl_res);
	 void load_accept_ex();
	 SocketState* new_socket_state();
	 WSAOVERLAPPED* new_overlapped();

	 void read_completed(BOOL resultOk, DWORD length,
		SocketState* socketState, WSAOVERLAPPED* ovl);
	
	 void start_accepting();

};

