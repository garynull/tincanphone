/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#include "Mutex.h"

#ifdef _WIN32
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <Windows.h>
#else
#	include <pthread.h>
#endif

namespace tincan {


#ifdef _WIN32
	Mutex::Mutex() : impl((void*)new CRITICAL_SECTION) { InitializeCriticalSection((CRITICAL_SECTION*)impl); }
	Mutex::~Mutex() { DeleteCriticalSection((CRITICAL_SECTION*)impl); delete (CRITICAL_SECTION*)impl; }
	void Mutex::lock() { EnterCriticalSection((CRITICAL_SECTION*)impl); }
	void Mutex::unlock() { LeaveCriticalSection((CRITICAL_SECTION*)impl); }
#else
	Mutex::Mutex() : impl((void*)new pthread_mutex_t) { pthread_mutex_init((pthread_mutex_t*)impl, 0); }
	Mutex::~Mutex() { pthread_mutex_destroy((pthread_mutex_t*)impl); delete (pthread_mutex_t*)impl; }
	void Mutex::lock()   { pthread_mutex_lock((pthread_mutex_t*)impl); }
	void Mutex::unlock() { pthread_mutex_unlock((pthread_mutex_t*)impl); }
#endif


}
