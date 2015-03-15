#pragma once

#include <windows.h>
#include <vector>
#include <mutex>
#include <condition_variable>

#define MAX_MSG_LEN 1500
#define SEND_THREAD_NUM 4
namespace std {
class thread;
}

struct MessageHeader {
	unsigned int messageLength; // Number of bytes in the message including this header
	unsigned int messageNumber;
};

class Client {
public:
	unsigned int numberOfMessagesSent;
	unsigned int numberOfMessagesReceived;

	Client(std::vector<std::pair<std::string, unsigned int>>&& serverAddressesAndPorts,
		bool nodelay);
	virtual ~Client();

	void sendMessage(MessageHeader* data);

	void sendMessage(std::string&& msg);

	void setVerbosity(bool verbose) {
		this->verbose = verbose;
	}

	bool getVerbosity() {
		return verbose;
	}

	void setMessageHandlerFunction(
			std::function<void(MessageHeader*)> function) {
		messageHandlerFunction = function;
	}

	std::function<void(MessageHeader*)> getDefaultMessageHandler();

private:
	const std::vector<std::pair<std::string, unsigned int>> serverAddressesAndPorts;

	int sock;
	
	std::mutex connectionMutex;

	const std::thread* receiverThread;

	int sendRoundRobinCounter;

	std::function<void(MessageHeader*)> messageHandlerFunction;

	bool verbose;

	const bool nodelay;

	void initWinsock();

	void createSocket();

	void sendData(char data[], unsigned int len);

	void doConnect();

	void receiveMessages();

	void reconnect();

	in_addr stringToIp(std::string);
};

