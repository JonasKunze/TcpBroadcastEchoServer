#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <iostream>


enum Configs {
	MAX_MSG_LEN = 1500,
	NUMBER_OF_BUFFS = 1000 // this makes 15 MB per client
};

struct MessageHeader {
	unsigned int messageLength; // Number of bytes in the message including this header
	unsigned int messageNumber;
};


struct AcceptState {
	SOCKET socket;
	char buf[100];
	bool isServerAccepted;
};

class SocketState
{
public:
	SOCKET socket;

	unsigned int buffReadPtr; // Next byte to be read
	unsigned int buffWritePtr; // Next byte to be written
	unsigned int nextMessagePtr; // always between the two other pointers -> you may not read this or higher bytes as they belong to an unfinished message
	unsigned int lastByteWritten;

	WSAOVERLAPPED* sendOverlapped;
	WSAOVERLAPPED* receiveOverlapped;
	WSABUF currentWriteBuff;
	WSABUF currentReadBuff;
	char* buff;
	const unsigned int buffSize;

	std::atomic<int> pendingOperations;
	bool toBeClosed;
	std::mutex closeMutex;
	bool ownedByWriteThread;


	unsigned int bytesMissingForCurrentMessage;
	char unfinishedMsgBuffer[MAX_MSG_LEN];

	/*
	 * Use a condition variable to wake up a writing thread waiting for a reading one to free up some memory
	 */
	std::mutex readMutex;
	std::condition_variable readCondVar;


	SocketState();

	virtual ~SocketState();

	WSABUF* getWritableBuff();

	void writeFinished();

	void findMessages();

	WSABUF* getReadableBuff();

	void readFinished();
};

