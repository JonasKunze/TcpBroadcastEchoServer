/*
* ThreadsafeQueue.h
*
* This class is based on the proposal at following blog post:
* http://blogs.msmvps.com/vandooren/2007/01/05/creating-a-thread-safe-producer-consumer-queue-in-c-without-using-locks/
*
* A thread safe consumer-producer queue. This means this queue is only thread safe if you have only one writer-thread and only one reader-thread.
*  Created on: Jan 5, 2012
*      Author: Jonas Kunze (kunze.jonas@gmail.com)
*/

#pragma once

#include <cstdint>
#include <thread>

#include <mutex>
#include <condition_variable>

template<class T> class ThreadsafeProducerConsumerQueue {
public:

	volatile uint32_t readPos_;
	volatile uint32_t writePos_;
	uint32_t Size_;
	T* Data;

	std::mutex readMutex;
	std::condition_variable readCondVar;

	std::mutex writeMutex;
	std::condition_variable writeCondVar;

	ThreadsafeProducerConsumerQueue(uint32_t size = 1000) :
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
	bool push(T&& element) {
 
		const uint32_t nextElement = (writePos_ + 1) % Size_;

		std::unique_lock<std::mutex> lock(readMutex);
		readCondVar.wait(lock, [this, nextElement]{
			return (readPos_ != nextElement);
		});
		/*
		while (readPos_ == nextElement) {
		std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
		*/
		Data[writePos_] = element;
		writePos_ = nextElement;
		writeCondVar.notify_one();
		return true;
	}

	/*
	* remove the oldest element from the circular queue. May only be called by one single thread (consumer)!
	*/
	void pop(T& element) {

		// Workaround: Sometimes we receive a notification even though the pointer did not change yet. 'volatile' 
		// seems not to be enough on Windows -> set a timeout
		std::unique_lock<std::mutex> lock(writeMutex);
		while (readPos_ == writePos_) {
			//std::this_thread::sleep_for(std::chrono::microseconds(1));
			writeCondVar.wait_for(lock, std::chrono::microseconds(10));
		}

		const uint32_t nextElement = (readPos_ + 1) % Size_;

		element = Data[readPos_];
		readPos_ = nextElement;

		readCondVar.notify_one();
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
