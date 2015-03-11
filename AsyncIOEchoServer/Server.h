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
	NUMBER_OF_BUFFS = 10000 // this makes 15 MB per client
};

struct MessageHeader {
	unsigned int messageLength; // Number of bytes in the message including this header
	unsigned int messageNumber;
};

struct SocketState {
	SOCKET socket;
	WSABUF buffs[NUMBER_OF_BUFFS];
	
	unsigned int buffReadPtr;
	unsigned int buffWritePtr;
	unsigned int acknowledgedWritePtr; // always between the two other pointers -> you may not read higher elements as they are not finished with writing yet
	std::atomic<int> ongoingOps;
	WSAOVERLAPPED* sendOverlapped;
	WSAOVERLAPPED* receiveOverlapped;	
	WSABUF* currentWriteBuff;
	char* allBuffsMemory;
	bool toBeClosed;
	
	unsigned int bytesMissingForCurrentMessage;
	char unfinishedMsgBuffer[BUFF_LEN];

	SocketState(){
		socket = -1;
		buffReadPtr = 0;
		buffWritePtr = 0;
		acknowledgedWritePtr = 0;

		ongoingOps = 0;
		sendOverlapped = nullptr;
		receiveOverlapped = nullptr;

		toBeClosed = false;

		bytesMissingForCurrentMessage = 0;

		// Minimize fragmentation my allocating one single buffer and split it then 
		allBuffsMemory = new char[NUMBER_OF_BUFFS*BUFF_LEN];

		for (auto& buff : buffs){
			buff.len = 0;
			buff.buf = allBuffsMemory;
			allBuffsMemory += BUFF_LEN;
		}
		allBuffsMemory -= NUMBER_OF_BUFFS*BUFF_LEN;
	}

	~SocketState() {
		delete[] allBuffsMemory; // delete the big memory block used by all buffers
	}

	/*
	Returns a pointer to a free buffer. This may only be called by one thread (producer)
	*/
	WSABUF* getWritableBuff(bool setCurrentWriteBuff=true) {
		const uint32_t nextElement = (buffWritePtr + 1) % NUMBER_OF_BUFFS; // next write element
		// wait until the element is available (the ring buffer is not full any more)
		while (nextElement == buffReadPtr) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		WSABUF* freeBuff = &buffs[buffWritePtr];
		freeBuff->len = BUFF_LEN;
		buffWritePtr = nextElement;

		if (setCurrentWriteBuff){
			currentWriteBuff = freeBuff;
		}
		return freeBuff;
	}

	/*
	Returns true if the last buffer returned by getWritableBuff was the last element of the Buffer array.
	*/
	bool isLastWritableBufferAtEndOfArray() {
		// if current write ptr is 0 the last getWriteTableBuff returned the last in the array
		return buffWritePtr == 0;
	}

	void writeFinished(){
		const uint32_t nextElement = (acknowledgedWritePtr + 1) % NUMBER_OF_BUFFS; // next write element
		// wait until the element is available (the ring buffer is not full any more)
		while (nextElement == buffReadPtr) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		
		acknowledgedWritePtr = nextElement;
	}

	/*
	* Returns the number of elements that may be read from the pointer on.  This may only be called by one thread (consumer)
	*/
	unsigned int getReadableBuffs(WSABUF*& startPointer) {
		// Wait as long as the ring buffer is empty
		while (buffReadPtr == acknowledgedWritePtr) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}

		unsigned int numberOfReadableElements = NUMBER_OF_BUFFS - buffReadPtr; // correct if all elements right of readPtr are valid
		if (acknowledgedWritePtr > buffReadPtr) {//Only read the elements between write and read ptr
			numberOfReadableElements = acknowledgedWritePtr - buffReadPtr;
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

	 void findMessages(SocketState* socketState);
	
	 void start_accepting();

};

