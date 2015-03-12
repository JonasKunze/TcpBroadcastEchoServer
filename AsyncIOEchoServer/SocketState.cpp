#include "SocketState.h"

SocketState::~SocketState() {
	delete[] buff; // delete the big memory block used by all buffers
}