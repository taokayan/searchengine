
#pragma once

#include "KKBase.h"
#include "KKFunctor.h"
#include "KKTuple.h"

class KKTaskable : public KKObject {
private:
	HANDLE m_thread;
	DWORD m_threadID;
	static DWORD async_(void *param);
public:
	inline KKTaskable() : m_thread(0), m_threadID(0) { }
	inline KKTaskable(const KKTaskable &t) : m_thread(0), m_threadID(0) { }
	inline bool async(size_t stackSize = 0) {
		HANDLE thread = ::CreateThread(NULL, //Choose default security
			stackSize, //Default stack size
			(LPTHREAD_START_ROUTINE)&async_, //Routine to execute
			(LPVOID) this, //Thread parameter
			0, //Immediately run the thread
			&m_threadID //Thread Id
			);
		HANDLE oldth = pointerAtomXch(m_thread, thread);
		if (oldth) {
			::WaitForSingleObject(oldth, 0x7FFFFFFF);
			::CloseHandle(oldth);
		}
		return (thread != NULL);
	}
	inline void join() {
		volatile HANDLE h = pointerAtomXch(m_thread, 0);
		if (h) {
			::WaitForSingleObject(h, 0x7FFFFFFF);
			::CloseHandle(h);
		}
	}

	// note: must call join at subclass KKTask0, KKTask1...
	//       can't call it here, because the vtable is deleted already !!!
	virtual ~KKTaskable() { }

	// abstract task implemented in KKTask0, KKTask1,...
	virtual void run() = 0;
};

#define DECL_KKTask(N) \
template<KKPreCList(typename R, typename P, N)> \
class KKTask##N : public KKTaskable { \
	typedef KKFn##N<KKPreCList(R, P, N)> Fn; \
	KKLocalRef<KKTuple##N<KKSufCList(P,KKObject,N)> > m_params; \
	Fn m_fn; \
public: \
	template <typename C> \
	KKTask##N(KKRef<C> &c, KKPreDCList(R (C::*f)(KKCList(P, N)), P, p, N)) { \
	  m_fn.set(c,f); \
	  m_params = new KKTuple##N<KKSufCList(P,KKObject,N)>(KKCList(p, N)); \
    } \
	template <typename C> \
	KKTask##N(C *c, KKPreDCList(R (C::*f)(KKCList(P, N)), P, p, N)) { \
	  m_fn.set(c,f); \
	  m_params = new KKTuple##N<KKSufCList(P,KKObject,N)>(KKCList(p, N)); \
    } \
	KKTask##N(KKPreDCList(R (*f)(KKCList(P, N)), P, p, N)) { \
	  m_fn.set(f); \
	  m_params = new KKTuple##N<KKSufCList(P,KKObject,N)>(KKCList(p, N)); \
    } \
	virtual void run() { \
		m_fn(KKCList(m_params->m_p, N)); \
	} \
	virtual ~KKTask##N() { \
	  join(); \
	} \
}

template <typename LambdaFn>
class KKTask_lambda : public KKTaskable {
	LambdaFn m_fn;
public:
	KKTask_lambda(LambdaFn &fn) : m_fn(fn) {}
	virtual void run() { m_fn(); }
	virtual ~KKTask_lambda() { join(); }
};

DECL_KKTask(0);
DECL_KKTask(1);
DECL_KKTask(2);
DECL_KKTask(3);
DECL_KKTask(4);
DECL_KKTask(5);
DECL_KKTask(6);
DECL_KKTask(7);
DECL_KKTask(8);
DECL_KKTask(9);
DECL_KKTask(10);
DECL_KKTask(11);
DECL_KKTask(12);
DECL_KKTask(13);
DECL_KKTask(14);
DECL_KKTask(15);
DECL_KKTask(16);

KKAssert(sizeof(KKTask0<void>) > 0);
KKAssert(sizeof(KKTask1<void, int>) > 0);
KKAssert(sizeof(KKTask2<void, int *, char &>) > 0);

#define DECL_KKTask_Set(N) \
template<KKPreCList(typename R, typename P, N)> \
	inline void set(KKPreDCList(R (*f)(KKCList(P, N)), P, p, N)) { \
	m_task = new KKTask##N<KKPreCList(R, P, N)>(KKPreCList(f, p, N)); } \
template<typename C, KKPreCList(typename R, typename P, N)> \
	inline void set(KKRef<C> &c, KKPreDCList(R (C::*f)(KKCList(P, N)), P, p, N)) { \
	m_task = new KKTask##N<KKPreCList(R, P, N)>(c, KKPreCList(f, p, N)); } \
template<typename C, KKPreCList(typename R, typename P, N)> \
	inline void set(C *c, KKPreDCList(R (C::*f)(KKCList(P, N)), P, p, N)) { \
	m_task = new KKTask##N<KKPreCList(R, P, N)>(c, KKPreCList(f, p, N)); } \
template<KKPreCList(typename R, typename P, N)> \
	inline KKTask(KKPreDCList(R (*f)(KKCList(P, N)), P, p, N)) { \
	set(KKPreCList(f, p, N)); } \
template<typename C, KKPreCList(typename R, typename P, N)> \
	inline KKTask(KKRef<C> &c, KKPreDCList(R (C::*f)(KKCList(P, N)), P, p, N)) { \
	set(c, KKPreCList(f, p, N)); } \
template<typename C, KKPreCList(typename R, typename P, N)> \
	inline KKTask(C *c, KKPreDCList(R (C::*f)(KKCList(P, N)), P, p, N)) { \
	set(c, KKPreCList(f, p, N)); } 


class KKTask {
private:
	KKRef<KKTaskable> m_task;
public:
	inline KKTask() { }
	inline KKTask(const KKTask &t) = default;
	inline KKTask(KKTask &&t) : m_task(t.m_task.xch(0)) { }

	inline KKTask& operator=(const KKTask &t) = default;
	inline KKTask& operator=(KKTask &&t) = default;

	inline bool operator == (const KKTask &t) const {
		return m_task.get() == t.m_task.get();
	}
	inline bool operator != (const KKTask &t) const {
		return m_task.get() != t.m_task.get();
	}
	inline bool operator < (const KKTask &t) const {
		return m_task.get() < t.m_task.get();
	}
	inline bool operator <= (const KKTask &t) const {
		return m_task.get() <= t.m_task.get();
	}
	inline bool operator > (const KKTask &t) const {
		return m_task.get() > t.m_task.get();
	}
	inline bool operator >= (const KKTask &t) const {
		return m_task.get() >= t.m_task.get();
	}
	inline void run() {
		KKLocalRef<KKTaskable> t = m_task;
		if (t) t->run();
	}
	inline bool async(size_t stackSize = 0) {
		KKLocalRef<KKTaskable> t = m_task;
		if (t) return t->async(stackSize);
		else return false;
	}
	inline void join() {
		KKLocalRef<KKTaskable> t = m_task;
		if (t) t->join();
	}

	enum Lambda_ { Lambda };
	template <typename T>
	KKTask(Lambda_, T &fn) {
		m_task = new KKTask_lambda<T>(fn);
	}
	template <typename T>
	void set(Lambda_, T &fn) {
		m_task = new KKTask_lambda<T>(fn);
	}

	DECL_KKTask_Set(0)
	DECL_KKTask_Set(1)
	DECL_KKTask_Set(2)
	DECL_KKTask_Set(3)
	DECL_KKTask_Set(4)
	DECL_KKTask_Set(5)
	DECL_KKTask_Set(6)
	DECL_KKTask_Set(7)
	DECL_KKTask_Set(8)
	DECL_KKTask_Set(9)
	DECL_KKTask_Set(10)
	DECL_KKTask_Set(11)
	DECL_KKTask_Set(12)
	DECL_KKTask_Set(13)
	DECL_KKTask_Set(14)
	DECL_KKTask_Set(15)
	DECL_KKTask_Set(16)
};
