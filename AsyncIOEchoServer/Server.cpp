#include "Server.h"

#include <iostream>
#include <vector>

//using namespace std; // don't use it because <mutex> redefines bind()

/*
 * portNumber: the port number the clients have to connect to
 * receiveAddres: defines the address/network device to which should be listened. Use INADDR_ANY to listen to any device
 * nodelay: if set to true the nagle's algorithm will be switched off
 */
Server::Server(unsigned int portNumber, unsigned long receiveAddress, bool nodelay) :
		GuidAcceptEx(WSAID_ACCEPTEX), readJobs(10000), writeJobs(10000), mySocketState(), portNumber(
				portNumber), receiveAddress(receiveAddress), nodelay(nodelay) {
	initWinsock();
	createIoCompletionPort();
	createSocket();
	loadAcceptEx();
	startAccepting();
}

Server::~Server() {
}

/*
 * Runs WSAStartup to initiate the usage of ws2
 */
void Server::initWinsock() {
	WSADATA wsaData;

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << "in WSAStartup" << std::endl;
		exit(1);
	}
}

/*
 * Initializes the instance variable completionPort with a new completion port
 */
void Server::createIoCompletionPort() {
	completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!completionPort) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << " in  CreateIoCompletionPort"
				<< std::endl;
		exit(1);
	}
}

/*
 * Initializes the instance variable mySocket with a new TCP/IP socket
 */
void Server::createSocket() {
	mySocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (mySocket == INVALID_SOCKET) {
		std::cerr << "Error creating listening socket!" << std::endl;
		exit(1);
	}

	mySocketState.socket = -1;

	if (CreateIoCompletionPort((HANDLE) mySocket, completionPort,
			(ULONG_PTR) & mySocketState, 0) != completionPort) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << " in mySocket" << std::endl;
		exit(1);
	}

	struct sockaddr_in sin;

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(receiveAddress);
	sin.sin_port = htons(portNumber);

	if (bind(mySocket, (SOCKADDR*) &sin, sizeof(sin)) == SOCKET_ERROR) {
		std::cerr << "Error in bind!" << std::endl;
		exit(1);
	}

	if (listen(mySocket, 100) == SOCKET_ERROR) {
		std::cerr << "Error in listen!" << std::endl;
		exit(1);
	}
	std::cout << "Started listening for connections on port " << portNumber
			<< std::endl;
}

/*
 * Starts reading from the given socket
 */
void Server::asyncRead(SocketState_ptr socketState) {
	DWORD flags = 0;
	socketState->pendingOperations++;
	if (WSARecv(socketState->socket, socketState->getWritableBuff(), 1, NULL,
			&flags, socketState->receiveOverlapped, NULL) == SOCKET_ERROR) {
		socketState->pendingOperations--;
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			std::cout << "Error " << err << " in WSARecv" << std::endl;
			closeConnection(socketState);
		}
	}
}

/*
 * Sends all available messages in the buffer of socketState to all connected clients asynchronously
 */
void Server::asyncBroadcast(SocketState_ptr socketState) {

	WSABUF* receivedMessages;

	// send as many messages as available
	while ((receivedMessages = socketState->getReadableBuff()) != nullptr) {

		std::unique_lock<std::mutex> lock(clientsMutex);
		for (auto& client : clients) {
			client->pendingOperations++;
			// Sends all buffs available to the current connected client
			if (WSASend(client->socket, receivedMessages, 1, NULL, 0,
					client->sendOverlapped, NULL) == SOCKET_ERROR) {
				client->pendingOperations--;
			}
		}
		socketState->readFinished();
	}
}

/*
 * Checks the state after a read operation and reinitializes the socket sta
 */
void Server::onSendComplete(BOOL resultOk, DWORD length,
		SocketState_ptr socketState) {
	memset(socketState->sendOverlapped, 0, sizeof(WSAOVERLAPPED));
	if (resultOk) {
		if (length <= 0) {
			std::cout << "Connection closed by client!" << std::endl;
			closeConnection(socketState);
		}
	} else // !resultOk, assumes connection was reset
	{
		int err = GetLastError();
		std::cerr << "Error " << err
				<< " on send, assuming connection was reset!" << std::endl;
		closeConnection(socketState);
	}
}

/*
 * Initializes a new socket state for the new connection and initiates a read operation on the new stream.
 * A new accept operation will also be initiated
 */
void Server::onAcceptComplete(BOOL resultOk, DWORD length,
		AcceptState* socketState) {

	SocketState_ptr newSocketState;

	if (resultOk) {
		std::cout << "* new connection created" << std::endl;

		// updates the context
		setsockopt(socketState->socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
				(char *) &mySocket, sizeof(mySocket));

		// associates new socket with completion port
		newSocketState = createNewSocketState();
		newSocketState->socket = socketState->socket;
		if (CreateIoCompletionPort((HANDLE) newSocketState->socket,
				completionPort, (ULONG_PTR) newSocketState.get() , 0)
				!= completionPort) {
			int err = WSAGetLastError();
			std::cerr << "Error " << err << " in CreateIoCompletionPort"
					<< std::endl;
			exit(1);
		}

		// starts receiving from the new connection
		asyncRead(newSocketState);

		// starts waiting for another connection request
		startAccepting();
	} else {
		int err = GetLastError();
		std::cerr << "Error (" << err << "," << length
				<< ") in accept, cleaning up and retrying!!!" << std::endl;
		closesocket(socketState->socket);
		socketState->socket = 0;
		startAccepting();
	}
}

/*
 * Returns a socket to be used for accepting connections
 */
SOCKET Server::createAcceptingSocket() {
	SOCKET acceptor = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (acceptor == INVALID_SOCKET) {
		std::cerr << "Error creating accept socket!" << std::endl;
		exit(1);
	}

	// deactivate nagle's algorithm
	if (setsockopt(mySocket, IPPROTO_TCP, TCP_NODELAY,
		(char *)&nodelay, sizeof(nodelay))
		== SOCKET_ERROR) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << " in setsockopt TCP_NODELAY"
			<< std::endl;
		exit(1);
	}
	return acceptor;
}

/*
 * Tries to close a connection or flags it as closed. This way the second thread will close it soon
 */
void Server::closeConnection(SocketState_ptr socketState) {
	//std::unique_lock<std::mutex> lock(socketState->closeMutex);
	if (!socketState->toBeClosed && socketState->pendingOperations <= 0) {
		std::cout << "Closing connection " << socketState->socket << std::endl;
		socketState->toBeClosed = true;
		
		std::unique_lock<std::mutex> lock(clientsMutex);
		clients.erase(socketState);

		// The socket will be closed in the SocketState destructor
	}
}

/*
 * Tries to get a new completion status and returns false in case of an error
 */
BOOL Server::getCompletionStatus(DWORD* length, SocketState_ptr* socketState,
		WSAOVERLAPPED** ovl_res) {
	BOOL resultOk;
	*ovl_res = NULL;
	*socketState = NULL;

	resultOk = GetQueuedCompletionStatus(completionPort, length,
			(PULONG_PTR) socketState, (WSAOVERLAPPED**) ovl_res, INFINITE);



	if (!*socketState || !*ovl_res) {
		std::cout << "Don't know what to do, aborting!!!" << std::endl;
		exit(1);
	}

	return resultOk;
}

/*
 * AcceptEx must be loaded at runtime!
 */
void Server::loadAcceptEx() {
	DWORD dwBytes;

	WSAIoctl(mySocket, SIO_GET_EXTENSION_FUNCTION_POINTER, &GuidAcceptEx,
		sizeof(GuidAcceptEx), &pfAcceptEx, sizeof(pfAcceptEx), &dwBytes,
		NULL,
		NULL);
}

/*
 * Generate a new SocketState object
 */
std::shared_ptr<SocketState> Server::createNewSocketState() {
	SocketState_ptr state(new SocketState());

	std::unique_lock<std::mutex> lock(clientsMutex);
	clients.insert(state);
	return state;
}

/*
 * Initiates a new write job (broadcast) and carries on reading
 */
void Server::onReadComplete(BOOL resultOk, DWORD length,
	SocketState_ptr socketState) {

	if (resultOk) {
		if (length > 0) {
			std::cout << "Received " << length << " B" << std::endl;

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
			closeConnection(socketState);
		}
	}
	else // !resultOk, assumes connection was reset
	{
		int err = GetLastError();
		std::cerr << "Error " << err
			<< " in recv, assuming connection wasw reset by client"
			<< std::endl;
		closeConnection(socketState);
	}
}

/*
 * The thread processing read operations (incoming messages)
 */
void Server::readThread() {
	SocketState_ptr socketState;
	while (true) {
		readJobs.pop(socketState);
		asyncRead(socketState);
	}
}

/*
 * The thread processing write operations (broadcasts)
 */
void Server::writeThread() {
	SocketState_ptr socketState;
	while (true) {
		writeJobs.pop(socketState);
		asyncBroadcast(socketState);
	}
}

/*
 * Spawns a read and a write thread and distributes finished IO jobs to those
 */
void Server::run() {
	std::thread readThread(&Server::readThread, this);
	std::thread writeThread(&Server::writeThread, this);

	DWORD length;
	BOOL resultOk;
	WSAOVERLAPPED* ovl_res;
	SocketState_ptr socketState;

	while (true) {
		resultOk = getCompletionStatus(&length, &socketState, &ovl_res);
		socketState->pendingOperations--;

		if (ovl_res == socketState->receiveOverlapped) {
			onReadComplete(resultOk, length, socketState);
		} else if (ovl_res == socketState->sendOverlapped) {
			onSendComplete(resultOk, length, socketState);
		} else if (socketState->sendOverlapped == nullptr) {
			std::cout << "New connection accepted" << std::endl;
			onAcceptComplete(resultOk, length,
					reinterpret_cast<AcceptState*>(socketState.get()));
		} else {
			std::cerr << "Unknown state! Aborting" << std::endl;
			exit(1);
		}
	}
}

/*
 * Starts accepting connections on mySocket
 */
void Server::startAccepting() {
	SOCKET acceptor = createAcceptingSocket();
	DWORD expected = sizeof(struct sockaddr_in) + 16;

	std::cout << "Started accepting connections..." << std::endl;

	// uses mySocket's completion key and overlapped structure
	mySocketState.socket = acceptor;
	memset(&mySocketOverlapped, 0, sizeof(WSAOVERLAPPED));

	// starts asynchronous accept
	if (!pfAcceptEx(mySocket, acceptor, mySocketState.buf, 0 /* no recv */,
			expected, expected, NULL, (WSAOVERLAPPED*) &mySocketOverlapped)) {
		int err = WSAGetLastError();
		if (err != ERROR_IO_PENDING) {
			std::cerr << "Error " << err << " in AcceptEx" << std::endl;
			exit(1);
		}
	}
}
