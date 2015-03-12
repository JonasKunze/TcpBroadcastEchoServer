#pragma once

#include "winsock2.h"
#include "mswsock.h"
#include <windows.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>

enum Configs {
	BUFF_LEN = 10,
	SERVER_ADDRESS = INADDR_LOOPBACK,
	SERVICE_PORT = 1234,
	NUMBER_OF_BUFFS = 3 // this makes 15 MB per client
};

struct MessageHeader {
	unsigned int messageLength; // Number of bytes in the message including this header
	unsigned int messageNumber;
};

class SocketState
{
public:
	virtual ~SocketState();

	SOCKET socket;

	unsigned int buffReadPtr; // Next byte to be read
	unsigned int buffWritePtr; // Next byte to be written
	unsigned int nextMessagePtr; // always between the two other pointers -> you may not read this or higher bytes as they belong to an unfinished message
	unsigned int lastByteWritten;

	std::atomic<int> ongoingOps;
	WSAOVERLAPPED* sendOverlapped;
	WSAOVERLAPPED* receiveOverlapped;
	WSABUF currentWriteBuff;
	WSABUF currentReadBuff;
	char* buff;
	const unsigned int buffSize;
	bool toBeClosed;

	unsigned int bytesMissingForCurrentMessage;
	char unfinishedMsgBuffer[BUFF_LEN];

	SocketState() : buffSize(NUMBER_OF_BUFFS*BUFF_LEN + 1) {
		socket = -1;

		// start at BUFF_LEN so that we have space to copy unfinished messages from the back later
		buffReadPtr = 0;
		buffWritePtr = 0;
		nextMessagePtr = 0;
		lastByteWritten = 0;

		ongoingOps = 0;
		sendOverlapped = nullptr;
		receiveOverlapped = nullptr;

		currentWriteBuff = { 0 };

		toBeClosed = false;

		bytesMissingForCurrentMessage = 0;

		// Minimize fragmentation by allocating one single buffer and split it then 

		buff = new char[buffSize];
	}


	void print(std::string name);
	/*
	Returns a pointer to a free buffer with maximum available size. This may only be called by one thread (producer)
	*/
	WSABUF* getWritableBuff() {
		print("getWritableBuf");

		unsigned int freeSpace = 0;

		if (buffWritePtr == buffSize) {
			while (buffReadPtr == 0){
				// wait until the space is free on the front of the buffer
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			buffWritePtr = 0;
		}

		while (freeSpace == 0) {
			if (buffWritePtr < buffReadPtr) { // write is left of read
				freeSpace = buffReadPtr - buffWritePtr - 1;
			}
			else { // write is right of read
				freeSpace = buffSize - buffWritePtr;
			}

			if (freeSpace == 0){
				// wait until enough space is free
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
		}

		currentWriteBuff.len = freeSpace;
		currentWriteBuff.buf = buff + buffWritePtr;

		print("getWritableBuf2");
		return &currentWriteBuff;
	}

	void writeFinished() {

		if (buffWritePtr == 53 && nextMessagePtr == 53) {
			std::cout << "!!" << std::endl;
		}

		print("writeFinished");
		buffWritePtr += currentWriteBuff.len;

		if (buffWritePtr>lastByteWritten){
			lastByteWritten = buffWritePtr - 1;
		}

		// calculate new buffer pointers and move unfinished messages to the beginning of the buffer
		findMessages();

		print("writeFinished2");
	}

	void findMessages() {

		unsigned int writtenBytes = buffWritePtr - nextMessagePtr; // number of bytes between ack byte and the last written one
		unsigned int bytesTillEndOfBuff = buffSize - nextMessagePtr;

		// Check if we jumped back to the beginning last time. In this case the next message starts at firstByteWritten
		if (nextMessagePtr > buffWritePtr) {
			writtenBytes = buffSize - nextMessagePtr;
		}

		MessageHeader* hdr = nullptr;
		for (; writtenBytes>=sizeof(MessageHeader);) { // Move from message to message
			hdr = reinterpret_cast<MessageHeader*>(buff + nextMessagePtr);
			if (hdr->messageLength > BUFF_LEN) {
				std::cerr << "Received message longer than the allowed maximum of " << BUFF_LEN << std::endl;
				exit(1);
			}
			if (hdr->messageLength <= writtenBytes) {
				nextMessagePtr += hdr->messageLength;

				if (hdr->messageLength == writtenBytes){
					break;
				}

				writtenBytes -= hdr->messageLength;
				bytesTillEndOfBuff -= hdr->messageLength;
				// Check if the  next header can even be complete
				if (writtenBytes < sizeof(MessageHeader)){
					break;
				}
			}
			else {
				break;
			}
		}


		// Check if enough space is left to the right or if we have to jump back to the beginning
		if (bytesTillEndOfBuff < sizeof(MessageHeader) || hdr->messageLength > bytesTillEndOfBuff) {
			while (buffReadPtr < writtenBytes || buffReadPtr > nextMessagePtr){
				// wait until the space is free on the front of the buffer
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
			// copy the beginning of the unfinished message to the front of the buffer (BUFF_LEN bytes are free there)
			memcpy(buff + 0, buff + nextMessagePtr, writtenBytes);
			buffWritePtr = writtenBytes;
			lastByteWritten -= writtenBytes;
			nextMessagePtr = 0;
		}
	}

	/*
	* Returns the number of elements that may be read from the pointer on.  This may only be called by one thread (consumer)
	*/
	WSABUF* getReadableBuff() {
		print("getReadableBuf");

		if (buffReadPtr == nextMessagePtr || buffReadPtr == lastByteWritten){
			return nullptr;
		}

		if (buffReadPtr >= lastByteWritten && buffReadPtr > nextMessagePtr) {
			buffReadPtr = 0;
			lastByteWritten = 0;
		}

		// wait as long as the buffer is empty
		if (buffReadPtr == nextMessagePtr){
			return nullptr;
		}

		currentReadBuff.buf = buff + buffReadPtr;

		if (buffReadPtr > nextMessagePtr && buffReadPtr != buffSize) { // read until end of buffer
			currentReadBuff.len = lastByteWritten - buffReadPtr + 1;
			lastByteWritten = 0;
		}
		else {
			currentReadBuff.len = nextMessagePtr - buffReadPtr;
		}

		MessageHeader* hdr = reinterpret_cast<MessageHeader*>(currentReadBuff.buf);
		std::cout << currentReadBuff.len << "!!" << hdr->messageLength << std::endl;
		print("getReadableBuf2");
		return &currentReadBuff;
	}

	/*
	Should be called after all elements accessed via getReadableBuffs() are read and may be overwritten.
	*/
	void readFinished() {
		print("readFinished");
		buffReadPtr += currentReadBuff.len;
		print("readFinished2");
	}
};

