/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#pragma once

#include "PhoneCommon.h"
#include <cassert>

namespace tincan {


class Mutex
{
public:
	Mutex();
	~Mutex();

	// Lock the mutex, or block until lock can be obtained
	void lock();

	// Release lock, duh
	void unlock();

protected:
	void* impl;
};


class Scopelock
{
public:
	Scopelock(Mutex& mtx) : mutex(&mtx) { mutex->lock(); }
	~Scopelock() { if (mutex) mutex->unlock(); }

	// Unlock the mutex "early"
	void unlock() { assert(mutex); mutex->unlock(); mutex = NULL; }

protected:
	Mutex* mutex;
};


}
