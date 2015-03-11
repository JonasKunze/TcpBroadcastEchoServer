#pragma once

#include <windows.h>
#include <vector>
#include <mutex>
#include <condition_variable>

#define BUFLEN 1500
namespace std {
	class thread;
}

struct MessageHeader {
	unsigned int messageLength; // Number of bytes in the message including this header
	unsigned int messageNumber;
};

class Client
{
public:
	Client(std::vector<std::string>&& serverAddresses, const unsigned int serverPort);
	virtual ~Client();

	void sendMessage(const char data[], const int len);
	void sendMessage(std::string&& msg);

	void setVerbosity(bool verbose) {
		this->verbose = verbose;
	}

	bool getVerbosity() {
		return verbose;
	}

private:
	const std::vector<std::string> serverAddresses;
	const unsigned int serverPort;
	int sock;
	struct sockaddr_in sin;
	std::mutex connectionMutex;

	const std::thread* receiverThread;

	unsigned int numberOfMessagesSent;
	unsigned int numberOfMessagesReceived;

	bool verbose;

	void initWinsock();

	void createSocket();

	void sendData(const char data[], const int len);

	/*
	* Loops over all server addresses until it manages to connect to one of them
	*/
	void doConnect();

	/**
	* Reads as many messages as available
	*/
	void receiveMessages();

	void reconnect();
};

