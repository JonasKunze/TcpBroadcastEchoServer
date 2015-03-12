#include "SocketState.h"

std::mutex logMutex;

SocketState::~SocketState() {
	delete[] buff; // delete the big memory block used by all buffers
}

void SocketState::print(std::string name){
	logMutex.lock();
	std::cout << name.c_str() << "\t\t" << buffReadPtr << "\t" << buffWritePtr << "\t" << nextMessagePtr << std::endl;
	logMutex.unlock();
}