#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <set>
#include <atomic>
#include <mutex>
#include <thread>


enum Configs {
	BUFF_LEN = 1500,
	SERVER_ADDRESS = INADDR_LOOPBACK,
	SERVICE_PORT = 1234,
	NUMBER_OF_BUFFS = 100
};



struct SocketState {
	SOCKET socket;
	WSABUF buffs[NUMBER_OF_BUFFS];
	
	unsigned int buffReadPtr;
	unsigned int buffWritePtr;
	std::atomic<int> ongoingOps;
	WSAOVERLAPPED* sendOverlapped;
	WSAOVERLAPPED* receiveOverlapped;	
	WSABUF* currentWritBuff;

	SocketState(){
		socket = -1;
		buffReadPtr = 0;
		buffWritePtr = 0;

		ongoingOps = 0;
		sendOverlapped = nullptr;
		receiveOverlapped = nullptr;
		for (auto& buff : buffs){
			buff.len = 0;
			buff.buf = new char[BUFF_LEN];
		}
	}

	/*
	Returns a pointer to a free buffer. This may only be called by one thread (producer)
	*/
	WSABUF* getWritableBuff() {
		const uint32_t nextElement = (buffWritePtr + 1) % NUMBER_OF_BUFFS; // next write element
		// wait until the element is available (the ring buffer is not full any more)
		while (nextElement == buffReadPtr) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		WSABUF* freeBuff = &buffs[buffWritePtr];
		freeBuff->len = BUFF_LEN;

		currentWritBuff = freeBuff;
		return freeBuff;
	}

	void writeFinished(){
		const uint32_t nextElement = (buffWritePtr + 1) % NUMBER_OF_BUFFS; // next write element
		// wait until the element is available (the ring buffer is not full any more)
		while (nextElement == buffReadPtr) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		
		buffWritePtr = nextElement;
		currentWritBuff = nullptr;
	}

	/*
	* Returns the number of elements that may be read from the pointer on.  This may only be called by one thread (consumer)
	*/
	unsigned int getReadableBuffs(WSABUF*& startPointer) {
		// Wait as long as the ring buffer is empty
		while (buffReadPtr == buffWritePtr) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}

		unsigned int numberOfReadableElements = NUMBER_OF_BUFFS - buffReadPtr - 1; // correct if all elements right of readPtr are valid
		if (buffWritePtr > buffReadPtr) {//Only read the elements between write and read ptr
			numberOfReadableElements = buffWritePtr - buffReadPtr - 1;
		}

		startPointer = &buffs[buffReadPtr];
		
		return numberOfReadableElements;
	}

	/*
	Should be called after all elements accessed via getReadableBuffs() are read and may be overwritten.  
	*/
	void readFinished(unsigned int numberOfElementsToPop) {
		buffReadPtr = (buffReadPtr + numberOfElementsToPop) % NUMBER_OF_BUFFS; // next element to be read
	}
};


class Server
{
public:
	Server();
	virtual ~Server();

	void run();

private:
	// the completion port
	HANDLE completionPort;

	// the listening socket
	SOCKET mySocket;
	SocketState mySocketState;
	WSAOVERLAPPED mySocketOverlapped;

	// utility: variables used to load the AcceptEx function
	LPFN_ACCEPTEX pfAcceptEx;
	GUID GuidAcceptEx;

	std::set<SocketState*> clients;
	std::mutex clientsMutex;

	void initWinsock();

	void create_io_completion_port();
	 
	void createSocket();

	void prepareSocket();

	void start_reading(SocketState* socketState);

	void startBroadcasting(SocketState* socketState);

	void write_completed(BOOL resultOk, DWORD length,
		SocketState* socketState);


	 void accept_completed(BOOL resultOk, DWORD length,
		 SocketState* socketState);
	 SOCKET create_accepting_socket();
	 void destroy_connection(SocketState* socketState);
	 BOOL get_completion_status(DWORD* length, SocketState** socketState,
		 WSAOVERLAPPED** ovl_res);
	 void load_accept_ex();
	 SocketState* new_socket_state();
	 WSAOVERLAPPED* new_overlapped();

	 void read_completed(BOOL resultOk, DWORD length,
		 SocketState* socketState);
	
	 void start_accepting();

};

