#ifndef GRAL_THREAD_HPP
#define GRAL_THREAD_HPP

#ifdef GRAL_WINDOWS
#include <Windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

namespace gral {

class Thread {
	#ifdef GRAL_WINDOWS
	HANDLE thread;
	template <class C, void (C::*M)()> static unsigned WINAPI thread_function(LPVOID user_data) {
		C *object = static_cast<C *>(user_data);
		(object->*M)();
		return 0;
	}
	Thread(unsigned (*function)(LPVOID), LPVOID user_data) {
		thread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, function, user_data, 0, nullptr));
	}
	#else
	pthread_t thread;
	template <class C, void (C::*M)()> static void *thread_function(void *user_data) {
		C *object = static_cast<C *>(user_data);
		(object->*M)();
		return nullptr;
	}
	Thread(void *(*function)(void *), void *user_data) {
		pthread_create(&thread, nullptr, function, user_data);
	}
	#endif
public:
	Thread(): thread(0) {}
	Thread(Thread const &) = delete;
	Thread(Thread &&thread): thread(thread.thread) {
		this->thread = thread.thread;
		thread.thread = 0;
	}
	~Thread() {
		if (thread) {
			#ifdef GRAL_WINDOWS
			CloseHandle(thread);
			#else
			pthread_detach(thread);
			#endif
		}
	}
	Thread &operator =(Thread const &) = delete;
	explicit operator bool() const {
		return thread;
	}
	template <class C, void (C::*M)()> static Thread create(C *object) {
		return Thread(&thread_function<C, M>, object);
	}
	void join() {
		#ifdef GRAL_WINDOWS
		WaitForSingleObject(thread, INFINITE);
		CloseHandle(thread);
		#else
		pthread_join(thread, NULL);
		#endif
		thread = 0;
	}
};

}

#endif
