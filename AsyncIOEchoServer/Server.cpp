#include "Server.h"

#include <iostream>

//using namespace std; // don't use it as <mutex> redefines bind


Server::Server() : GuidAcceptEx(WSAID_ACCEPTEX) {
	initWinsock();
	create_io_completion_port();
	createSocket();
	prepareSocket();
	load_accept_ex();
	start_accepting();
}

Server::~Server() {
}

void Server::initWinsock() {
	WSADATA wsaData;

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << "in WSAStartup" << std::endl;
		exit(1);
	}
}

void Server::create_io_completion_port() {
	completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!completionPort) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << " in  CreateIoCompletionPort" << std::endl; 
		exit(1);
	}
}

void Server::createSocket() {
	mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mySocket == INVALID_SOCKET) {
		std::cerr << "Error creating listening socket!" << std::endl;
		exit(1);
	}

	// for use by AcceptEx
	mySocketState.socket = -1;
	mySocketState.operation = OP_ACCEPT;

	if (CreateIoCompletionPort((HANDLE)mySocket, completionPort,
		(ULONG_PTR)&mySocketState, 0) != completionPort)	{
		int err = WSAGetLastError();
		std::cerr << "Error " << err << " in mySocket" << std::endl;
		exit(1);
	}
}

void Server::prepareSocket() {
	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(SERVER_ADDRESS);
	sin.sin_port = htons(SERVICE_PORT);

	if (bind(mySocket, (SOCKADDR*)&sin, sizeof(sin)) == SOCKET_ERROR) {
		std::cerr << "Error in bind!" << std::endl;
		exit(1);
	}

	if (listen(mySocket, 100) == SOCKET_ERROR)	{
		std::cerr << "Error in listen!" << std::endl;
		exit(1);
	}
	std::cout << "* started listening for connection requests..." << std::endl;
}

void Server::run()
{
	DWORD length;
	BOOL resultOk;
	WSAOVERLAPPED* ovl_res;
	SocketState* socketState;

	for (;;)
	{
		resultOk = get_completion_status(&length, &socketState, &ovl_res);

		if (socketState->markDisconnect){
			destroy_connection(socketState, ovl_res);
			continue;
		}

		switch (socketState->operation)
		{
		case OP_ACCEPT:
			std::cout << "Operation ACCEPT completed" << std::endl;
			accept_completed(resultOk, length, socketState, ovl_res);
			break;

		case OP_READ:
			read_completed(resultOk, length, socketState, ovl_res);
			break;

		case OP_WRITE:
			write_completed(resultOk, length, socketState, ovl_res);
			break;

		default:
			std::cerr << "Error, unknown operation!" << std::endl;
			destroy_connection(socketState, ovl_res);
			break;
		}
	}
}

void Server::start_reading(SocketState* socketState, WSAOVERLAPPED* ovl) {
	socketState->activeOperations++;

	DWORD flags = 0;
	WSABUF wsabuf = { MAX_BUF, socketState->buf };

	memset(ovl, 0, sizeof(WSAOVERLAPPED));
	socketState->operation = OP_READ;
	if (WSARecv(socketState->socket, &wsabuf, 1, NULL, &flags, ovl, NULL)
		== SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			std::cout << "Error " << err << " in WSARecv" << std::endl;
			destroy_connection(socketState, ovl);
		}
	}
}

void Server::startBroadcasting(SocketState* socketState, WSAOVERLAPPED* ovl) {
	WSABUF wsabuf = { socketState->length, socketState->buf };

	//std::std::cout << "Sending " << socketState->length << " B: " << socketState->buf << std::std::endl;
	SocketState* state = new SocketState();
	state->operation = OP_READ;
	memcpy(state, socketState, sizeof(SocketState));
	start_reading(state, new_overlapped());

	std::unique_lock<std::mutex> lock(clientsMutex);
	for (auto& client : clients) {
		client->activeOperations++;

		memset(ovl, 0, sizeof(WSAOVERLAPPED));
		client->operation = OP_WRITE;

		if (WSASend(client->socket, &wsabuf, 1, NULL, 0, ovl, NULL)
			== SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING)
			{
				std::cout << "Error " << err << " in WSASend" << std::endl;
				destroy_connection(client, ovl);
			}
		}
	}
}

void Server::write_completed(BOOL resultOk, DWORD length,
	SocketState* socketState, WSAOVERLAPPED* ovl)
{
	socketState->activeOperations--;

	if (resultOk)
	{
		if (length > 0)
		{
			//start_reading(socketState, ovl); // starts another read
		}
		else // length == 0 (strange!)
		{
			std::cout << "* connection closed by client!" << std::endl;
			destroy_connection(socketState, ovl);
		}
	}
	else // !resultOk, assumes connection was reset
	{
		int err = GetLastError();
		std::cerr << "Error " << err << " on send, assuming connection was reset!" << std::endl;
		destroy_connection(socketState, ovl);
	}
}
 

void Server::accept_completed(BOOL resultOk, DWORD length,
	SocketState* socketState, WSAOVERLAPPED* ovl) {
	socketState->activeOperations--;

	SocketState* newSocketState;

	if (resultOk) {
		std::cout << "* new connection created" << std::endl;

		// updates the context
		setsockopt(socketState->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&mySocket, sizeof(mySocket));

		// associates new socket with completion port
		newSocketState = new_socket_state();
		newSocketState->socket = socketState->socket;
		if (CreateIoCompletionPort((HANDLE)newSocketState->socket, completionPort,
			(ULONG_PTR)newSocketState, 0) != completionPort)
		{
			int err = WSAGetLastError();
			std::cerr << "Error " << err << " in CreateIoCompletionPort" << std::endl;
			exit(1);
		}

		// starts receiving from the new connection
		start_reading(newSocketState, new_overlapped());

		// starts waiting for another connection request
		start_accepting();
	}  else {
		int err = GetLastError();
		std::cerr << "Error (" << err << "," << length << ") in accept, cleaning up and retrying!!!" << std::endl;
		closesocket(socketState->socket);
		start_accepting();
	}
}


SOCKET Server::create_accepting_socket()
{
	SOCKET acceptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (acceptor == INVALID_SOCKET)
	{
		std::cerr << "Error creating accept socket!" << std::endl;
		exit(1);
	}
	return acceptor;
}


void Server::destroy_connection(SocketState* socketState, WSAOVERLAPPED* ovl)
{
	if (socketState->activeOperations == 0) {
		std::unique_lock<std::mutex> lock(clientsMutex);
		clients.erase(socketState);
		closesocket(socketState->socket);
		delete socketState;
		delete ovl;
	}
	else {
		socketState->markDisconnect = true;
	}
}



BOOL Server::get_completion_status(DWORD* length, SocketState** socketState,
	WSAOVERLAPPED** ovl_res) {
	BOOL resultOk;
	*ovl_res = NULL;
	*socketState = NULL;

	resultOk = GetQueuedCompletionStatus(completionPort, length, (PULONG_PTR)socketState,
		ovl_res, INFINITE);

	if (!resultOk)
	{
		DWORD err = GetLastError();
		std::cerr << "Error " << err << " getting completion port status!!!" << std::endl;
		destroy_connection(*socketState, *ovl_res);
	}

	if (!*socketState || !*ovl_res)
	{
		std::cout << "* don't know what to do, aborting!!!" << std::endl;
		exit(1);
	}

	return resultOk;
}


void Server::load_accept_ex() 
{
	DWORD dwBytes;

	// black magic for me
	WSAIoctl(mySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx,
		sizeof(GuidAcceptEx), &pfAcceptEx, sizeof(pfAcceptEx), &dwBytes, NULL,
		NULL);
}

SocketState* Server::new_socket_state() {
	SocketState* state = reinterpret_cast<SocketState*>(calloc(1, sizeof(SocketState)));
	std::unique_lock<std::mutex> lock(clientsMutex);
	clients.insert(state);
	return state;
}


WSAOVERLAPPED* Server::new_overlapped() {
	return (WSAOVERLAPPED*)calloc(1, sizeof(WSAOVERLAPPED));
}

void Server::read_completed(BOOL resultOk, DWORD length,
	SocketState* socketState, WSAOVERLAPPED* ovl) {
	socketState->activeOperations--;

	if (resultOk){
		if (length > 0)	{
			//std::std::cout << "Received " << socketState->buf << std::std::endl;

			// starts another write
			socketState->length = length;
			startBroadcasting(socketState, ovl);
		}
		else // length == 0
		{
			std::cout << "* connection closed by client" << std::endl;
			destroy_connection(socketState, ovl);
		}
	}
	else // !resultOk, assumes connection was reset
	{
		int err = GetLastError();
		std::cerr << "Error " << err << " in recv, assuming connection was reset by client" << std::endl;
		destroy_connection(socketState, ovl);
	}
}

void Server::start_accepting() {
	SOCKET acceptor = create_accepting_socket();
	DWORD expected = sizeof(struct sockaddr_in) + 16;

	std::cout << "Started accepting connections..." << std::endl;

	// uses mySocket's completion key and overlapped structure
	mySocketState.socket = acceptor;
	memset(&mySocketOverlapped, 0, sizeof(WSAOVERLAPPED));
	mySocketState.activeOperations++;
	// starts asynchronous accept
	if (!pfAcceptEx(mySocket, acceptor, mySocketState.buf, 0 /* no recv */,
		expected, expected, NULL, &mySocketOverlapped))
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			std::cerr << "Error " << err << " in AcceptEx" << std::endl;
			exit(1);
		}
	}
}