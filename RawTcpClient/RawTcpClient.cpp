#include <vector>
#include <string>
#include <iostream>

#include "Utils.h"
#include "Client.h"


using namespace std;

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
			if (sscanf_s(msg.data() + 5, " %d %d", &msgNum, &len)!=2){
				cerr << "Bad input format! Please use something like 'storm 10000 1000" << endl;
				continue;
			}
						
			char* buf = new char[len];
			for (int i = 0; i < len; i++){
				buf[i] = 'a'+(i%26);
			}

			cout << "\tSending data storm with " << msgNum << " messages of " << len << " B each" << endl;
			
			bool oldVerbose = client.getVerbosity();
			client.setVerbosity(false);
			long long start = Utils::getCurrentMillis();
			for (int i = 0; i != msgNum; i++) {
				client.sendMessage(buf, len);
			}

			long long time = Utils::getCurrentMillis() - start;
			if (time == 0) {
				time = 1;
			}

			delete[] buf;

			long long datarate = (msgNum*len) / time;
			long long msgrate = datarate * 1000 / len;
			cout << endl << "Sent " << msgNum << " messages within " << time << " ms (" << datarate << " kB/s, " << msgrate << " messages/s)" << endl;

			Sleep(100);
			client.setVerbosity(oldVerbose);
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