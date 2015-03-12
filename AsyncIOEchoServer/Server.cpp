#include "Server.h"

#include <iostream>
#include <vector>

//using namespace std; // don't use it because <mutex> redefines bind()


Server::Server() : GuidAcceptEx(WSAID_ACCEPTEX), readJobs(1000), writeJobs(1000), mySocketState(){
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

	if (CreateIoCompletionPort((HANDLE)mySocket, completionPort,
		(ULONG_PTR)&mySocketState, 0) != completionPort) {
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


void Server::start_reading(SocketState* socketState) {
	DWORD flags = 0;

	socketState->ongoingOps++;

	if (WSARecv(socketState->socket, socketState->getWritableBuff(), 1, NULL, &flags, socketState->receiveOverlapped, NULL)
		== SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			std::cout << "Error " << err << " in WSARecv" << std::endl;
			destroy_connection(socketState);
		}
	}
}

void Server::startBroadcasting(SocketState* socketState) {
	WSABUF* receivedMessages;
	while ((receivedMessages = socketState->getReadableBuff())!=nullptr) {
		char* buff = new char[receivedMessages->len];
		memcpy(buff, receivedMessages->buf, receivedMessages->len);
		WSABUF broadcastMessages = { receivedMessages->len, buff };
		socketState->readFinished();

		std::unique_lock<std::mutex> lock(clientsMutex);
		for (auto& client : clients) {
			if (client->toBeClosed) {
				continue;
			}
			client->ongoingOps++;

			// Sends all buffs available to the current connected client
			if (WSASend(client->socket, &broadcastMessages, 1, NULL, 0, client->sendOverlapped, NULL)
				== SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				if (err != WSA_IO_PENDING)
				{
					std::cout << "Error " << err << " in WSASend " << client->socket << "\t" << client->ongoingOps << "\t" << client->toBeClosed << std::endl;
					destroy_connection(client);
				}
			}
		}
		delete[] buff;
	}
}

void Server::write_completed(BOOL resultOk, DWORD length,
	SocketState* socketState)
{
	socketState->ongoingOps--;

	memset(socketState->sendOverlapped, 0, sizeof(WSAOVERLAPPED));
	if (resultOk)
	{
		if (length <= 0)
		{
			std::cout << "Connection closed by client!" << std::endl;
			destroy_connection(socketState);
		}
	}
	else // !resultOk, assumes connection was reset
	{
		int err = GetLastError();
		std::cerr << "Error " << err << " on send, assuming connection was reset!" << std::endl;
		destroy_connection(socketState);
	}
}
 

void Server::accept_completed(BOOL resultOk, DWORD length,
	AcceptState* socketState) {

	SocketState* newSocketState;

	if (resultOk) {
		std::cout << "* new connection created" << std::endl;

		// updates the context
		setsockopt(socketState->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
			(char *)&mySocket, sizeof(mySocket));

		// deactivate nagle's algorithm
		//int flag = 1;
		//int result = setsockopt(mySocket, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));


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
		start_reading(newSocketState);

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


void Server::destroy_connection(SocketState* socketState)
{	
	if (socketState->ongoingOps == 0){
		std::unique_lock<std::mutex> lock(clientsMutex);
		clients.erase(socketState);
		if (socketState->socket > 0){
			closesocket(socketState->socket);
		}
		delete socketState;
	}
	else {
		socketState->toBeClosed = true;
	}
}



BOOL Server::get_completion_status(DWORD* length, SocketState** socketState,
	WSAOVERLAPPED** ovl_res) {
	BOOL resultOk;
	*ovl_res = NULL;
	*socketState = NULL;

	resultOk = GetQueuedCompletionStatus(completionPort, length, (PULONG_PTR)socketState,
		(WSAOVERLAPPED**)ovl_res, INFINITE);

	if (!resultOk)
	{
		DWORD err = GetLastError();
		//std::cerr << "Error " << err << " getting completion port status!!!" << std::endl;
		destroy_connection(*socketState);
	}

	if (!*socketState || !*ovl_res)
	{
		std::cout << "Don't know what to do, aborting!!!" << std::endl;
		exit(1);
	}

	return resultOk;
}


void Server::load_accept_ex() 
{
	DWORD dwBytes;

	WSAIoctl(mySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx,
		sizeof(GuidAcceptEx), &pfAcceptEx, sizeof(pfAcceptEx), &dwBytes, NULL,
		NULL);
}

SocketState* Server::new_socket_state() {
	SocketState* state = new SocketState();
		
	std::unique_lock<std::mutex> lock(clientsMutex);
	clients.insert(state);
	return state;
}


void Server::read_completed(BOOL resultOk, DWORD length,
	SocketState* socketState) {
	socketState->ongoingOps--;

	if (resultOk){
		if (length > 0)	{
			//std::cout << "Received " << socketState->currentWriteBuff.buf+8 << std::endl;
						
			memset(socketState->receiveOverlapped, 0, sizeof(WSAOVERLAPPED));

			socketState->currentWriteBuff.len = length;
			socketState->writeFinished();


			// carry on reading and broadcast the messages
			readJobs.push(socketState);
			writeJobs.push(socketState);
		}
		else // length == 0
		{
			std::cout << "Connection closed by client" << std::endl;
			destroy_connection(socketState);
		}
	}
	else // !resultOk, assumes connection was reset
	{
		int err = GetLastError();
		std::cerr << "Error " << err << " in recv, assuming connection wasw reset by client" << std::endl;
		destroy_connection(socketState);
	}
}

void Server::readThread() {
	SocketState* socketState;
	while (true){
		readJobs.pop(socketState);
		start_reading(socketState);
	}
}

void Server::writeThread() {
	SocketState* socketState;
	while (true){
		writeJobs.pop(socketState);
		startBroadcasting(socketState);
	}
}

void Server::run()
{
	std::thread readThread(&Server::readThread, this);
	std::thread writeThread(&Server::writeThread, this);

	DWORD length;
	BOOL resultOk;
	WSAOVERLAPPED* ovl_res;
	SocketState* socketState;

	while (true)
	{
		resultOk = get_completion_status(&length, &socketState, &ovl_res);
		if (!resultOk || socketState->toBeClosed){
			destroy_connection(socketState);
			continue;
		}

		if (ovl_res == socketState->receiveOverlapped){
			//std::cout << "Receive finished" << std::endl;
			read_completed(resultOk, length, socketState);
		}
		else if (ovl_res == socketState->sendOverlapped){
			//std::cout << "Send finished" << std::endl;
			write_completed(resultOk, length, socketState);
		}
		else if (socketState->sendOverlapped == nullptr){
			std::cout << "New connection accepted" << std::endl;
			accept_completed(resultOk, length, reinterpret_cast<AcceptState*>(socketState));
		}
		else {
			std::cerr << "Unknown state! Aborting" << std::endl;
			exit(1);
		}
	}
}

void Server::start_accepting() {
	SOCKET acceptor = create_accepting_socket();
	DWORD expected = sizeof(struct sockaddr_in) + 16;

	std::cout << "Started accepting connections..." << std::endl;

	// uses mySocket's completion key and overlapped structure
	mySocketState.socket = acceptor;
	memset(&mySocketOverlapped, 0, sizeof(WSAOVERLAPPED));

	// starts asynchronous accept
	
	if (!pfAcceptEx(mySocket, acceptor, mySocketState.buf, 0 /* no recv */,
		expected, expected, NULL, (WSAOVERLAPPED*)&mySocketOverlapped))
	{
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING)
		{
			std::cerr << "Error " << err << " in AcceptEx" << std::endl;
			exit(1);
		}
	}
}