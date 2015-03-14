#include "Client.h"

#include <iostream>
#include <stdlib.h>
#include <thread>

using namespace std;

/*
 * A client can be used to send messages to any of the provided servers and receive responses from the same server.
 * As soon as the connection get's close by the remote a client will automatically try to connect to any other
 * server from the provided server list
 */
Client::Client(
		std::vector<std::pair<std::string, unsigned int>>&& serverAddressesAndPorts,
		bool nodelay) :
		serverAddressesAndPorts(std::move(serverAddressesAndPorts)), numberOfMessagesSent(
				0), numberOfMessagesReceived(0), sock(-1), verbose(true), nodelay(
				nodelay) {
	// Seed for randomization of server connections
	srand((unsigned int) time(NULL));

	initWinsock();
	reconnect();

	setMessageHandlerFunction(getDefaultMessageHandler());
	receiverThread = new std::thread(&Client::receiveMessages, this);
}

Client::~Client() {
	if (sock > 0) {
		cout << "Disconnecting" << endl;
		if (closesocket(sock) != 0) {
			int err = WSAGetLastError();
			cerr << "Error " << err << " in closesocket" << endl;
		}
	}
}

/*
 * Runs WSAStartup to initiate the usage of ws2
 */
void Client::initWinsock() {
	WSADATA wsaData;

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
		int err = WSAGetLastError();
		cerr << "Error " << err << "in WSAStartup" << endl;
		exit(1);
	}
}

/*
 * Crates a new TCP/IP socket
 */
void Client::createSocket() {
	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		int err = WSAGetLastError();
		printf("* error %d creating socket\n", err);
		exit(1);
	}

	// deactivate nagle's algorithm
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(char *)&nodelay, sizeof(nodelay))
		== SOCKET_ERROR) {
		int err = WSAGetLastError();
		std::cerr << "Error " << err << " in setsockopt TCP_NODELAY"
			<< std::endl;
		exit(1);
	}
}

/*
 * Closes the current connection and connects to any other random server
 */
void Client::reconnect() {
	if (connectionMutex.try_lock()) {
		if (sock > 0) {
			cout << "Disconnecting" << endl;
			if (closesocket(sock) != 0) {
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

/*
 * Loops over all known server addresses until it manages to connect to one of them
 */
void Client::doConnect() {

	// Trying all servers in an infinite loop starting with any random server
	for (unsigned int serverID = rand();; serverID++) {
		sin.sin_family = AF_INET;

		auto& addressAndPort = serverAddressesAndPorts[serverID
				% serverAddressesAndPorts.size()];

		//sin.sin_addr.s_addr = inet_addr(addressAndPort.first.c_str());
		sin.sin_addr = stringToIp(addressAndPort.first);
		sin.sin_port = htons(addressAndPort.second);

		cout << "Trying to connect to server " << addressAndPort.first.c_str()
				<< ":" << addressAndPort.second << endl;
		if (connect(sock, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
			int err = WSAGetLastError();
			cerr << "Error " << err << " in connect" << endl;
			continue;
		}
		cout << "Connection established!" << endl;
		return;
	}
}

/**
 * Starts reading from the socket and finding messages in the data stream
 */
void Client::receiveMessages() {
	const unsigned int bufferSize = MAX_MSG_LEN * 100;
	char buf[bufferSize];

	char* bufPtr = buf;
	unsigned int bufferFree = bufferSize;
	MessageHeader* header = nullptr;

	while (true) {
		while (bufferFree > 0) {
			int len = recv(sock, bufPtr, bufferFree, 0);

			if (len > 0) {
				if (verbose) {
					cout << "Received " << len << " B" << endl;
				}
				if (!header) {
					header = reinterpret_cast<MessageHeader*>(bufPtr);
				}

				bufferFree -= len;
				bufPtr += len;

				// check if a message is complete
				if (header->messageLength <= bufferSize - bufferFree) {
					break;
				}

				continue;
			}

			// Error handling
			if (len == 0) {
				cout << "Connection closed by server" << endl;
			} else {
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
		while (bufferFree < bufferSize) {
			header = reinterpret_cast<MessageHeader*>(bufPtr);
			unsigned int bytesRemaining = bufferSize - bufferFree;
			if (header->messageLength > 0
					&& header->messageLength <= bytesRemaining) {
				numberOfMessagesReceived++;

				/*
				 * Here comes the code processing incoming messages
				 */
				messageHandlerFunction(header);

				// 'free' the message buffer and allow to override it
				bufferFree += header->messageLength;
				bufPtr += header->messageLength;
				header = nullptr;
			} else { // the last message hasn't been received completely yet
					 // move the message to the beginning of the buffer to free up some space for more data
				memcpy(buf, bufPtr, bytesRemaining);
				bufPtr = buf + bytesRemaining;
				bufferFree = bufferSize - bytesRemaining;
				break;
			}
		}
		// move back to the beginning of the receive buffer if no unfinished message has been found
		if (header == nullptr) {
			bufPtr = buf;
			bufferFree = bufferSize;
		}
	}
}

/*
 * The Default message handler is used automatically if setMessageHandlerFunction has not been called. It
 * will print out useful messages about the received message
 */
std::function<void(MessageHeader*)> Client::getDefaultMessageHandler() {
	return [&](MessageHeader* header) {
		if (numberOfMessagesReceived % 1000 == 0) {
			cout << "Received " << numberOfMessagesReceived << " messages" << endl;
		}

		if (verbose) {
			// finish string in case \0 was not send (which I don't do)
			char buf[MAX_MSG_LEN];
			memcpy(buf, ((char*)header) + sizeof(MessageHeader), header->messageLength - sizeof(MessageHeader));
			buf[header->messageLength - sizeof(MessageHeader)] = '\0';

			cout << "Received message " << header->messageNumber << " with " << header->messageLength << " B: " << buf << std::endl;
		}
	};
}

/*
 * Sends a message with the given string and a corresponding header
 */
void Client::sendMessage(std::string&& msg) {
	char* buf = new char[msg.length() + sizeof(MessageHeader)];

	MessageHeader* header = reinterpret_cast<MessageHeader*>(buf);
	header->messageLength = msg.length() + sizeof(MessageHeader);
	header->messageNumber = numberOfMessagesSent++;

	memcpy(buf + sizeof(MessageHeader), msg.data(), msg.length());

	sendMessage(header);
	delete[] buf;
}

/*
 * Sends a message as is
 */
void Client::sendMessage(MessageHeader* data) {
	sendData(reinterpret_cast<char*>(data), data->messageLength);
}

/*
 * Sends raw data as is
 */
void Client::sendData(char* data, unsigned int len) {
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

in_addr Client::stringToIp(std::string address) {
	hostent * record = gethostbyname(address.c_str());
	if (record == NULL) {
		cerr << "Server " << address.c_str() << " in unavailable" << endl;
		return in_addr();
	}
	return *((in_addr*) record->h_addr);
}

