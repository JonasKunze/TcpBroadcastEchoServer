/*
* ThreadsafeQueue.h
*
* This class is based on the proposal at following blog post:
* na62://msmvps.com/blogs/vandooren/archive/2007/01/05/creating-a-thread-safe-producer-consumer-queue-in-c-without-using-locks.aspx
*
* A thread safe consumer-producer queue. This means this queue is only thread safe if you have only one writer-thread and only one reader-thread.
*  Created on: Jan 5, 2012
*      Author: Jonas Kunze (kunze.jonas@gmail.com)
*/

#pragma once

#include <cstdint>
#include <thread>

template<class T> class ThreadsafeProducerConsumerQueue {
public:

	volatile uint32_t readPos_;
	volatile uint32_t writePos_;
	uint32_t Size_;
	T* Data;

	ThreadsafeProducerConsumerQueue(uint32_t size) :
		Size_(size) {
		readPos_ = 0;
		writePos_ = 0;
		Data = new T[Size_];
	}

	~ThreadsafeProducerConsumerQueue() {
		delete[] Data;
	}

	T print() {
		return Data[readPos_];
	}

	/*
	* Push a new element into the circular queue. May only be called by one single thread (producer)!
	*/
	bool push(T& element) {
		const uint32_t nextElement = (writePos_ + 1) % Size_;
		while (readPos_ == nextElement) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		
		Data[writePos_] = element;
		writePos_ = nextElement;
		return true;
	}

	/*
	* remove the oldest element from the circular queue. May only be called by one single thread (consumer)!
	*/
	void pop(T& element) {
		while (readPos_ == writePos_) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
		const uint32_t nextElement = (readPos_ + 1) % Size_;

		element = Data[readPos_];
		readPos_ = nextElement;
	}

	uint32_t size() {
		return Size_;
	}

	uint32_t getCurrentLength() {
		if (readPos_ <= writePos_) {
			return writePos_ - readPos_;
		}
		else {
			return Size_ - readPos_ + writePos_;
		}
	}
};
