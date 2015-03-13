#pragma once

#include <windows.h>
#include <vector>
#include <mutex>
#include <condition_variable>

#define MAX_MSG_LEN 1500
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
	unsigned int numberOfMessagesSent;
	unsigned int numberOfMessagesReceived;

	Client(std::vector<std::string>&& serverAddresses, const unsigned int serverPort);
	virtual ~Client();

	void sendMessage(MessageHeader* data);
	void sendMessage(std::string&& msg);

	void setVerbosity(bool verbose) {
		this->verbose = verbose;
	}

	bool getVerbosity() {
		return verbose;
	}

	void setMessageHandlerFunction(std::function<void(MessageHeader*)> function){
		messageHandlerFunction = function;
	}

	std::function<void(MessageHeader*)> getDefaultMessageHandler();
	
private:
	const std::vector<std::string> serverAddresses;
	const unsigned int serverPort;
	int sock;
	struct sockaddr_in sin;
	std::mutex connectionMutex;

	const std::thread* receiverThread;

	std::function<void(MessageHeader*)> messageHandlerFunction;

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

