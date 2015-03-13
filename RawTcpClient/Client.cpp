#include "Client.h"

#include <iostream>
#include <stdlib.h>
#include <thread>

using namespace std;

Client::Client(std::vector<std::string>&& _serverAddresses, const unsigned int serverPort) : 
serverAddresses(std::move(_serverAddresses)), serverPort(serverPort), numberOfMessagesSent(0), numberOfMessagesReceived(0), sock(-1), verbose(true){
	initWinsock();
	reconnect();

	setMessageHandlerFunction(getDefaultMessageHandler());
	receiverThread = new std::thread(&Client::receiveMessages, this);
}

Client::~Client() {
	if (sock > 0) {
		cout << "Disconnecting" << endl;
		if (closesocket(sock) != 0){
			int err = WSAGetLastError();
			cerr << "Error " << err << " in closesocket" << endl;
		}
	}
}

std::function<void(MessageHeader*)> Client::getDefaultMessageHandler() {
	return  [&](MessageHeader* header) {
		if (numberOfMessagesReceived % 1000 == 0) {
			cout << "Received " << numberOfMessagesReceived << " messages" << endl;
		}

		if (verbose) {
			// finish string in case \0 was not send (which I don't do)
			((char*)header)[header->messageLength] = '\0';
			cout << "Received message " << header->messageNumber << " with " << header->messageLength << " B: " << ((char*)header) + sizeof(MessageHeader) << std::endl;
		}
	};
}

void Client::initWinsock() {
	WSADATA wsaData;

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		cerr << "Error " << err << "in WSAStartup" << endl;
		exit(1);
	}
}

void Client::createSocket()	{
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		int err = WSAGetLastError();
		printf("* error %d creating socket\n", err);
		exit(1);
	}

	// deactivate nagle's algorithm
	int flag = 1;
	int result = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int)); 
}

void Client::doConnect() {
	
	// Trying all servers in an infinite loop starting with any random server
	for (unsigned int serverID = rand();; serverID++) {
		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr(serverAddresses[serverID % serverAddresses.size()].c_str());
		sin.sin_port = htons(serverPort);

		cout << "Trying to connect to server " << serverAddresses[serverID % serverAddresses.size()].c_str() << endl;
		if (connect(sock, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
			int err = WSAGetLastError();
			cerr << "Error " << err << " in connect" << endl;
			continue;
		}
		cout << "Connection established!" << endl;
		return;
	}
}

void Client::reconnect() {
	if (connectionMutex.try_lock()){
		if (sock > 0) {
			cout << "Disconnecting" << endl;
			if (closesocket(sock) != 0){
				int err = WSAGetLastError();
				cerr << "Error " << err << " in closesocket" << endl;
			}
		}

		sock = -1;
		createSocket();
		doConnect();
		connectionMutex.unlock();
	} else {
		// Wait for the other thread to reconnect
		std::unique_lock<std::mutex> lock(connectionMutex);
	}
}

void Client::receiveMessages() {
	char buf[MAX_MSG_LEN];

	char* bufPtr = buf;
	unsigned int bufferFree = MAX_MSG_LEN;
	MessageHeader* header = nullptr;

	while (true) {
		while (bufferFree > 0) {
			int len = recv(sock, bufPtr, bufferFree, 0);

			if (len > 0) {
				if (!header) {
					header = reinterpret_cast<MessageHeader*>(bufPtr);
				}

				bufferFree -= len;
				bufPtr += len;
			
				// check if a message is complete
				if (header->messageLength <= MAX_MSG_LEN - bufferFree){
					break;
				}

				continue;
			}

			// Error handling
			if (len == 0) {
				cout << "Connection closed by server" << endl;
			}
			else {
				int err = WSAGetLastError();
				cerr << "Error " << err << " in recv" << endl;
			}

			// connect to another server or reconnect
			reconnect();
		}

		/*
		* Find all messages within the data
		*/
		bufPtr = buf;
		while (bufferFree < MAX_MSG_LEN) {
			header = reinterpret_cast<MessageHeader*>(bufPtr);
			unsigned int bytesRemaining = MAX_MSG_LEN - bufferFree;
			if (header->messageLength <= bytesRemaining) {
				numberOfMessagesReceived++;

				/*
				 * Here comes the code processing incoming messages
				*/
				messageHandlerFunction(header);

				// 'free' the message buffer and allow to override it
				bufferFree += header->messageLength;
				bufPtr += header->messageLength;
				header = nullptr;
			}
			else { // the last message hasn't been received completely yet
				// move the message to the beginning of the buffer to free up some space for more data
				memcpy(buf, bufPtr, bytesRemaining);
				bufPtr = buf + bytesRemaining;
				bufferFree = MAX_MSG_LEN - bytesRemaining;
				break;
			}
		}
		// move back to the beginning of the receive buffer if no unfinished message has been found
		if (header == nullptr) {
			bufPtr = buf;
			bufferFree = MAX_MSG_LEN;
		}
	}
}
void Client::sendMessage(std::string&& msg){
	char* buf = new char[msg.length() + sizeof(MessageHeader)];
	
	MessageHeader* header = reinterpret_cast<MessageHeader*>(buf);
	header->messageLength = msg.length() + sizeof(MessageHeader);
	header->messageNumber = numberOfMessagesSent++;

	memcpy(buf + sizeof(MessageHeader), msg.data(), msg.length());

	sendMessage(header);
	delete[] buf;
}

void Client::sendMessage(MessageHeader* data) {
	//char headderBuffer[sizeof(MessageHeader)];
	
	sendData(reinterpret_cast<char*>(data), data->messageLength);
	
	// print status feedback
	if (numberOfMessagesSent % 1000 == 0) {
		cout << "." << flush;
	}
}

void Client::sendData(const char* data, const int len) {
	unsigned int remainingBytes = len;

	while (remainingBytes > 0) {
		int sentBytes = send(sock, data, remainingBytes, 0);

		if (sentBytes > 0) {
			remainingBytes -= sentBytes;
			data += sentBytes;
			continue;
		}

		// Error handling
		if (sentBytes == 0) {
			cout << "Connection closed by server" << endl;
		} else {
			int err = WSAGetLastError();
			cerr << "Error " << err << " in send" << endl;
		}

		// connect to another server or reconnect
		reconnect();
	}
}