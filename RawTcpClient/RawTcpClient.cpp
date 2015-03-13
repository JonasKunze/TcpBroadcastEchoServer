#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <condition_variable>

#include "Utils.h"
#include "Client.h"
#include "Options.h"

using namespace std;

/*
 * Sends msgNum messages with msgLen Byte each to the server and measures the send data and message rate
 */
void runStormTest(Client& client, int msgLen, int msgNum) {
	MessageHeader* data = reinterpret_cast<MessageHeader*>(new char[msgLen]);
	data->messageLength = msgLen;

	cout << "\tSending data storm with " << msgNum << " messages of " << msgLen
			<< " B each" << endl;
	const unsigned int lastMessageNumber = client.numberOfMessagesReceived
			+ msgNum;

	// mute the client
	bool oldVerbose = client.getVerbosity();
	client.setVerbosity(false);

	// Send msgNum messages and measure the time
	long long start = Utils::getCurrentMillis();
	for (int i = 0; i != msgNum; i++) {
		client.sendMessage(data);
	}
	long long time = Utils::getCurrentMillis() - start;

	delete[] data;

	// devision by zero if we took less then 1 ms
	if (time == 0) {
		time = 1;
	}
	long long datarate = (msgNum * msgLen) / time;
	long long msgrate = datarate * 1000 / msgLen;

	// Wait until all responses have been received
	while (client.numberOfMessagesReceived < lastMessageNumber) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	cout << endl << "Sent " << msgNum << " messages within " << time << " ms ("
			<< datarate << " kB/s, " << msgrate << " messages/s)" << endl;

	client.setVerbosity(oldVerbose);
}

/*
 * Measured the round trip time latency my sending a message of length msgLen every time
 * another message has been received (ping pong) and starting with a single message. The
 * rtt is approximated by the time for all round trips divided by the number of round trips
 */
void runRttTest(Client& client, int msgLen) {
	if (msgLen < sizeof(MessageHeader) + 1) {
		cout << "msgLen must be at least " << sizeof(MessageHeader) + 1 << endl;
		return;
	}
	cout << "Measuring round trip time with messages of " << msgLen << " B each"
			<< endl;

	MessageHeader* data = reinterpret_cast<MessageHeader*>(new char[msgLen]);
	data->messageLength = msgLen;

	const unsigned int msgNum = 1000000;
	const unsigned int lastMessageNumber = client.numberOfMessagesReceived
			+ msgNum;
	long long endTime = 0;

	// Set the client message handler to send a message every time another message has been received
	// until enough messages have been received
	client.setMessageHandlerFunction([&](MessageHeader* msg) {
		if (client.numberOfMessagesReceived < lastMessageNumber) {
			client.sendMessage(data);
		}
		else {
			endTime = Utils::getCurrentMillis();
			client.setMessageHandlerFunction(client.getDefaultMessageHandler());
		}
	});

	// Start ping pong by sending one single message
	long long start = Utils::getCurrentMillis();
	client.sendMessage(data);

	// Wait until the last message has been received. Conditoin_variable seems not to work with std::function
	while (endTime == 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	float rtt = (endTime - start) / (float) msgNum;

	cout << "Average rtt for " << msgNum << " messages was " << rtt << "ms"
			<< endl;
}

void main(int argc, char *argv[]) {
	Options::initialize(argc, argv);

	vector<pair<string, unsigned int>> servers;
	servers.push_back(std::make_pair("127.0.0.1", 1234));

	Client client(std::move(Options::servers));

	std::string msg;
	while (true) {
		cout << "Please enter a message to be sent or 'help': ";
		getline(std::cin, msg);

		if (msg.compare("quit") == 0) {
			break;
		} else if (msg.compare("help") == 0) {
			cout << "\tFollowing commands can be used:" << endl;
			cout << "\t\tstorm <msgNum> <msglength>" << endl;
			cout << "\t\t\tMeasure bandwidth/message rate" << endl;
			cout << "\t\trtt <msglength>" << endl;
			cout << "\t\t\tMeasure the round trip time" << endl;
			cout << "\t\tverbose <1or0>" << endl;
			cout << "\t\t\tSet verbosity (hide message 'Received message...')"
					<< endl;
			cout << "\t\tquit" << endl;
			cout << "\t\t\tGoodbye!" << endl;
		} else if (msg.compare(0, 5, "storm") == 0) {
			int len = 100;
			int msgNum = 1000;
			if (sscanf_s(msg.data() + 5, " %d %d", &msgNum, &len) != 2) {
				cerr
						<< "Bad input format! Please use something like 'storm 10000 1000"
						<< endl;
				continue;
			}

			runStormTest(client, len, msgNum);
		} else if (msg.compare(0, 3, "rtt") == 0) {
			int msgLen = 100;
			if (sscanf_s(msg.data() + 3, " %d", &msgLen) != 1) {
				cerr << "Bad input format! Please use something like 'rtt 10"
						<< endl;
				continue;
			}
			runRttTest(client, msgLen);
		} else if (msg.compare(0, 7, "verbose") == 0) {
			int verbose;
			if (sscanf_s(msg.data() + 7, " %d", &verbose) != 1) {
				cerr << "Bad input format! Please use something like 'verbose 1"
						<< endl;
				continue;
			}
			cout << "\tSetting verbosity to " << verbose << endl;

			client.setVerbosity(verbose != 0);
		} else {
			client.sendMessage(std::move(msg));
			Sleep(100);
		}
	}
}
