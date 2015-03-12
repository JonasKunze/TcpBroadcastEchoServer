#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <condition_variable>

#include "Utils.h"
#include "Client.h"


using namespace std;

void runStormTest(Client& client, int msgLen, int msgNum){
	char* buf = new char[msgLen];
	for (int i = 0; i < msgLen; i++){
		buf[i] = 'a' + (i % 26);
	}

	cout << "\tSending data storm with " << msgNum << " messages of " << msgLen << " B each" << endl;

	bool oldVerbose = client.getVerbosity();
	client.setVerbosity(false);
	long long start = Utils::getCurrentMillis();
	for (int i = 0; i != msgNum; i++) {
		client.sendMessage(buf, msgLen);
	}

	long long time = Utils::getCurrentMillis() - start;
	if (time == 0) {
		time = 1;
	}

	delete[] buf;

	long long datarate = (msgNum*msgLen) / time;
	long long msgrate = datarate * 1000 / msgLen;
	cout << endl << "Sent " << msgNum << " messages within " << time << " ms (" << datarate << " kB/s, " << msgrate << " messages/s)" << endl;

	Sleep(100);
	client.setVerbosity(oldVerbose);
}

void runRttTest(Client& client, int msgLen) {
	cout << "Measuring round trip time with messages of " << msgLen << " B each" << endl;
	
	std::mutex mutex;
	std::condition_variable condVar;
	std::unique_lock<std::mutex> lock(mutex);

	char* buf = new char[msgLen];
	const unsigned int msgNum = 100;
	const unsigned int lastMessageNumber = client.numberOfMessagesReceived + msgNum;
	long long endTime;
	client.setMessageHandlerFunction([&](MessageHeader* msg){ 
		if (client.numberOfMessagesReceived < lastMessageNumber) {
			client.sendMessage(buf, msgLen);
		}
		else {
			endTime = Utils::getCurrentMillis();
			client.setMessageHandlerFunction(client.getDefaultMessageHandler());

			// notify runRttTest
			std::unique_lock<std::mutex> lock(mutex);
			condVar.notify_all();
		}
	});

	long long start = Utils::getCurrentMillis();
	client.sendMessage(buf, msgLen);

	// Wait until the last message has been received
	condVar.wait(lock);

	float rtt = 1000*(endTime - start)/msgNum;

	cout << "Average rtt for " << msgNum << " messages was " << rtt << "µs" << endl;
}

void main() {
	vector<string> servers;
	//servers.push_back("192.168.178.20");
	servers.push_back("127.0.0.1");

	Client client(std::move(servers), 1234);


	std::string msg;
	while (true) {
		cout << "Please enter a message to be sent or 'help': ";
		getline(std::cin, msg);

		if (msg.compare("quit") == 0) {
			break;
		}
		else if (msg.compare("help") == 0)
		{
			cout << "\tFollowing commands can be used:" << endl;
			cout << "\t\tstorm <msgNum> <length>" << endl;
			cout << "\t\tverbose <1or0>" << endl;
			cout << "\t\tquit" << endl;
		} 
		else if (msg.compare(0,5, "storm") == 0) 
		{
			int len = 100;
			int msgNum = 1000;
			if (sscanf_s(msg.data() + 5, " %d %d", &msgNum, &len) != 2){
				cerr << "Bad input format! Please use something like 'storm 10000 1000" << endl;
				continue;
			}

			runStormTest(client, len, msgNum);
		} 
		else if (msg.compare(0, 3, "rtt") == 0)
		{
			int msgLen = 100;
			if (sscanf_s(msg.data() + 3, " %d", &msgLen) != 1){
				cerr << "Bad input format! Please use something like 'rtt 10" << endl;
				continue;
			}
			runRttTest(client, msgLen);			
		}
		else if (msg.compare(0, 7, "verbose") == 0)
		{
			int verbose;
			if (sscanf_s(msg.data() + 7, " %d", &verbose) != 1){
				cerr << "Bad input format! Please use something like 'verbose 1" << endl;
				continue;
			}
			cout << "\tSetting verbosity to " << verbose << endl;
			
			client.setVerbosity(verbose != 0);
		}
		else
		{
			client.sendMessage(std::move(msg));
			Sleep(100);
		}
	}
}