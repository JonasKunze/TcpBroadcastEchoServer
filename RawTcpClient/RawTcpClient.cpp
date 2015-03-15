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
void runStormTest(Client& client, unsigned int msgLen, unsigned int msgNum, unsigned int threadNum) {
	if (msgLen < sizeof(MessageHeader) + 1) {
		cout << "msgLen must be at least " << sizeof(MessageHeader) + 1 << endl;
		return;
	}

	MessageHeader* data = reinterpret_cast<MessageHeader*>(new char[msgLen]);
	data->messageLength = msgLen;

	cout << "Sending data storm with " << msgNum << " messages of " << msgLen
			<< " B each" << endl;

	cout << "Please make sure that the connected server is running without --noEcho and press return to start the test!" << endl;
	string dummy;
	getline(std::cin, dummy);

	const unsigned int lastMessageNumber = client.numberOfMessagesReceived
			+ msgNum;

	// mute the client
	bool oldVerbose = client.getVerbosity();
	client.setVerbosity(false);

	/*
	 * Send msgNum messages and measure the time
	 */
	std::condition_variable condVar;
	std::mutex mutex;

	int numberOfThreads = 1;
	bool start = false;
	std::vector<std::thread> threadPool;
	for (int i = 0; i != numberOfThreads; i++){
		threadPool.push_back(std::thread([&](){
			// wait for the start signal
			{
				std::unique_lock<std::mutex> lock(mutex);
				condVar.wait(lock, [&]{
					return start; 
				});
			}
			for (int i = 0; i != msgNum / numberOfThreads; i++) {
				data->messageNumber = i;
				client.sendMessage(data);
			}
		}));	
	}
	
	// send start signal
	start = true;
	condVar.notify_all();
	long long startTime = Utils::getCurrentMillis();

	// Wait until all threads have finished
	for (auto& thread: threadPool){
		thread.join();
	}

	long long time = Utils::getCurrentMillis() - startTime;

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

	cout << "Please make sure that the connected server is running without --noEcho and press return to start the test!" << endl;
	string dummy;
	getline(std::cin, dummy);

	// mute the client
	bool oldVerbose = client.getVerbosity();
	client.setVerbosity(false);

	MessageHeader* data = reinterpret_cast<MessageHeader*>(new char[msgLen]);
	data->messageLength = msgLen;

	const unsigned int msgNum = 1000;
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

			// Switch back to the default message handler
			client.setMessageHandlerFunction(client.getDefaultMessageHandler());
		}
	});

	// Start ping pong by sending one single message
	long long start = Utils::getCurrentMillis();
	client.sendMessage(data);

	// Wait until the last message has been received. condition_variable seems not to work with std::function
	// so we have to poll for endTime
	while (endTime == 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	float rtt = 1000*((endTime) - start) / (float) msgNum;

	cout << endl << "Average rtt for " << msgNum << " messages was " << rtt << " microseconds"
			<< endl;
	client.setVerbosity(oldVerbose);
}

void main(int argc, char *argv[]) {
	Options::initialize(argc, argv);

	Client client(std::move(Options::servers), Options::nodelay);

	std::string msg;
	while (true) {
		cout << "Please enter a message to be sent or 'help': ";
		getline(std::cin, msg);

		if (msg.compare("quit") == 0) {
			break;
		} else if (msg.compare("help") == 0) {
			cout << "\tFollowing commands can be used:" << endl;
			cout << "\t\tstorm <msgNum> <msglength> <sendThreadNum>" << endl;
			cout << "\t\t\tMeasure bandwidth/message rate" << endl;
			cout << "\t\trtt <msglength>" << endl;
			cout << "\t\t\tMeasure the round trip time" << endl;
			cout << "\t\tverbose <1or0>" << endl;
			cout << "\t\t\tSet verbosity (hide message 'Received message...')"
					<< endl;
			cout << "\t\tquit" << endl;
			cout << "\t\t\tGoodbye!" << endl;
		} else if (msg.compare(0, 5, "storm") == 0) {
			unsigned int len = 100;
			unsigned int msgNum = 1000;
			unsigned int sendThreadNum = 4;
			if (sscanf_s(msg.data() + 5, " %d %d %d", &msgNum, &len, &sendThreadNum) != 3) {
				cerr
						<< "Bad input format! Please use something like 'storm 10000 1000 8"
						<< endl;
				continue;
			}

			runStormTest(client, len, msgNum, sendThreadNum);
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
