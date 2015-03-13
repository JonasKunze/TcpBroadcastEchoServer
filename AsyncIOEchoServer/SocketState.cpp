#include "SocketState.h"

SocketState::SocketState() :
		buffSize(NUMBER_OF_BUFFS * MAX_MSG_LEN + 1) {
	socket = -1;

	// start at MAX_MSG_LEN so that we have space to copy unfinished messages from the back later
	buffReadPtr = 0;
	buffWritePtr = 0;
	nextMessagePtr = 0;
	lastByteWritten = 0;

	sendOverlapped = nullptr;
	receiveOverlapped = nullptr;

	currentWriteBuff = {0};

	toBeClosed = false;

	bytesMissingForCurrentMessage = 0;

	sendOverlapped = (WSAOVERLAPPED*) calloc(1, sizeof(WSAOVERLAPPED));
	receiveOverlapped = (WSAOVERLAPPED*) calloc(1, sizeof(WSAOVERLAPPED));

	// Minimize fragmentation by allocating one single buffer and split it then

	buff = new char[buffSize];
}

SocketState::~SocketState() {
	delete[] buff; // delete the big memory block used by all buffers

	delete sendOverlapped;
	delete receiveOverlapped;
}

/*
 * Returns a pointer to a free buffer with maximum available size.
 * This may only be called by one thread (producer)
 */
WSABUF* SocketState::getWritableBuff() {

	unsigned int freeSpace = 0;

	if (buffWritePtr == buffSize) {
		while (buffReadPtr == 0) {
			// wait until the space is free on the front of the buffer
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		buffWritePtr = 0;
	}

	while (freeSpace == 0) {
		if (buffWritePtr < buffReadPtr) { // write is left of read
			freeSpace = buffReadPtr - buffWritePtr - 1;
		} else { // write is right of read
			freeSpace = buffSize - buffWritePtr;
		}

		if (freeSpace == 0) {
			// wait until enough space is free
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
	}

	currentWriteBuff.len = freeSpace;
	currentWriteBuff.buf = buff + buffWritePtr;

	return &currentWriteBuff;
}

/*
 * Must be called after the buffer accessed via getWritableBuff() is written and may be read
 * This may only be called by one thread (producer)
 */
void SocketState::writeFinished() {
	buffWritePtr += currentWriteBuff.len;

	if (buffWritePtr > lastByteWritten) {
		lastByteWritten = buffWritePtr - 1;
	}

	// calculate new buffer pointers and move unfinished messages to the beginning of the buffer
	findMessages();
}

/*
 * Goes through the new written buffer and moves the nextMessagePtr behind the last finished message found
 */
void SocketState::findMessages() {

	unsigned int writtenBytes = buffWritePtr - nextMessagePtr; // number of bytes between ack byte and the last written one
	unsigned int bytesTillEndOfBuff = buffSize - nextMessagePtr;

	// Check if we jumped back to the beginning last time. In this case the next message starts at firstByteWritten
	if (nextMessagePtr > buffWritePtr) {
		writtenBytes = buffSize - nextMessagePtr;
	}

	MessageHeader* hdr = nullptr;
	for (; writtenBytes >= sizeof(MessageHeader);) { // Move from message to message
		hdr = reinterpret_cast<MessageHeader*>(buff + nextMessagePtr);

		if (hdr->messageLength > MAX_MSG_LEN) {
			// TODO: Disconnect this client instead of shutting down the server
			std::cerr << "Received message longer than the allowed maximum of "
					<< MAX_MSG_LEN << std::endl;
			exit(1);
		}
		if (hdr->messageLength <= writtenBytes) {
			nextMessagePtr += hdr->messageLength;

			if (hdr->messageLength == writtenBytes) {
				break;
			}

			writtenBytes -= hdr->messageLength;
			bytesTillEndOfBuff -= hdr->messageLength;
			// Check if the  next header can even be complete
			if (writtenBytes < sizeof(MessageHeader)) {
				break;
			}
		} else {
			break;
		}
	}

	// Check if enough space is left to the right or if we have to jump back to the beginning
	if (bytesTillEndOfBuff < sizeof(MessageHeader)
			|| hdr->messageLength > bytesTillEndOfBuff) {
		// wait until the space is free on the front of the buffer
		std::unique_lock<std::mutex> lock(readMutex);
		readCondVar.wait(lock,
				[this, writtenBytes] {
					return (buffReadPtr >= writtenBytes && buffReadPtr <= nextMessagePtr);
				});

		/*
		 while (buffReadPtr < writtenBytes || buffReadPtr > nextMessagePtr){
		 // wait until the space is free on the front of the buffer
		 std::unique_lock<std::mutex> lock(readMutex);
		 }
		 */

		// copy the beginning of the unfinished message to the front of the buffer (MAX_MSG_LEN bytes are free there)
		memcpy(buff + 0, buff + nextMessagePtr, writtenBytes);
		buffWritePtr = writtenBytes;
		lastByteWritten -= writtenBytes;
		nextMessagePtr = 0;
	}
}

/*
* Returns a buffer that has not been read yet
* This may only be called by one thread (consumer)
*/
WSABUF* SocketState::getReadableBuff() {

	if (buffReadPtr == nextMessagePtr || buffReadPtr == lastByteWritten) {
		return nullptr;
	}

	if (buffReadPtr >= lastByteWritten && buffReadPtr > nextMessagePtr) {
		if (nextMessagePtr == 0) {
			return nullptr;
		}
		buffReadPtr = 0;
		lastByteWritten = 0;
	}

	// wait as long as the buffer is empty
	if (buffReadPtr == nextMessagePtr) {
		return nullptr;
	}

	currentReadBuff.buf = buff + buffReadPtr;

	if (buffReadPtr > nextMessagePtr && buffReadPtr != buffSize) { // read until end of buffer
		currentReadBuff.len = lastByteWritten - buffReadPtr + 1;
		lastByteWritten = 0;
	} else {
		currentReadBuff.len = nextMessagePtr - buffReadPtr;
	}

	MessageHeader* hdr = reinterpret_cast<MessageHeader*>(currentReadBuff.buf);
	return &currentReadBuff;
}

/*
 * Should be called after the buffer accessed via getReadableBuff() is read and may be overwritten
 * This may only be called by one thread (consumer)
 */
void SocketState::readFinished() {
	buffReadPtr += currentReadBuff.len;
	readCondVar.notify_all();
}
