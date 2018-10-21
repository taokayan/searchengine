
#pragma once

#include <stdlib.h>
#include <Windows.h>

#include "KKBase.h"

typedef CRITICAL_SECTION KKLock_LockT;
typedef DWORD KKLock_TidT;
#define KKLock_Init(l) ::InitializeCriticalSection(&(l))
#define KKLock_Final(l) ::DeleteCriticalSection(&(l))
#define KKLock_Enter(l) ::EnterCriticalSection(&(l))
#define KKLock_Leave(l) ::LeaveCriticalSection(&(l))
#define KKLock_TryEnter(l) ::TryEnterCriticalSection(&(l))
#define KKLock_CurTid() ::GetCurrentThreadId()
#define KKLock_Yield() ::SwitchToThread()

#define KKLock_Counter 0

template <typename T> class KKLockGuard;
class KKObject;

class KKLock {
	friend class KKLockGuard<KKLock>;
	friend class KKObject;
	template <typename T> friend class KKRef;
private:
	KKLock_LockT m_lock;

#if KKLock_Counter
	KKLock_TidT m_tid;
	int m_count;
#endif

public:
	inline KKLock()
	{ 
#if KKLock_Counter
		m_count = 0;
#endif
		KKLock_Init(m_lock); 
	}
	
	inline ~KKLock() { KKLock_Final(m_lock); }

private:
	KKLock(const KKLock &); // cannot duplicate
	KKLock &operator=(const KKLock &); // cannot assign
public:

#if KKLock_Counter
	inline int getCount() { return m_count; }
#endif

	inline void lock() {

#if KKLock_Counter
		if (m_tid && KKLock_CurTid() == m_tid) { m_count++; return; }
#endif
		KKLock_Enter(m_lock); 

#if KKLock_Counter
		m_tid = KKLock_CurTid();
		m_count++;
#endif
	}
	inline bool tryLock() { 

#if KKLock_Counter
		if (m_tid && KKLock_CurTid() == m_tid) { m_count++; return true; }
#endif
		if (KKLock_TryEnter(m_lock)) {
#if KKLock_Counter
			m_tid = KKLock_CurTid();
			m_count++;
#endif
			return true;
		} else return false;
	}
	inline void unlock() {
#if KKLock_Counter
		if (m_count > 1) { m_count--; return; }
		m_count = 0; 
		m_tid = 0;
#endif
		KKLock_Leave(m_lock);
	}
};

class KKNoLock {
public:
	inline void lock() { }
	inline bool tryLock() { return true; }
	inline void unlock() { }
};

class KKTicketLock {
	volatile unsigned __int32 m_ticket;
	int m_count;
	KKLock_TidT m_tid;

	inline void sleep_(KKLock_TidT id, int sc) {
		if (sc <= 0) return;
		if (((sc - 1) ^ sc) == 0) return;
		if (sc > 12) { ::Sleep(1); return; }
		for(int i = 0; i < (1 << sc); i++) ::Sleep(0);
	}

public:
	inline KKTicketLock() : m_ticket(0), m_count(0), m_tid(0) { }
	inline void lock() {
		if (m_tid && KKLock_CurTid() == m_tid) { m_count++; return; }
		volatile unsigned __int32 old = int32AtomXchAdd(m_ticket, 0x00010000);
		volatile unsigned __int32 myticket = (old >> 16) & 0xffff;
		volatile unsigned __int32 current = old & 0xffff;
		int sc = 0; KKLock_TidT tid = KKLock_CurTid();
		do {
			if (myticket == current) {
				m_tid = tid;
				m_count++;
				return;
			}
			sleep_(tid, sc++);
			current = m_ticket & 0xffff;
		} while(1);
	}
	inline bool tryLock() {
		if (m_tid && KKLock_CurTid() == m_tid) { m_count++; return true; }
		while (1) {
			volatile unsigned __int32 old = m_ticket;
			volatile unsigned __int32 myticket = (old >> 16) & 0xffff;
			volatile unsigned __int32 current = old & 0xffff;
			if (myticket != current) return false;
			if (int32AtomCmpXch(m_ticket, old + 0x00010000, old) == old) {
				m_tid = KKLock_CurTid();
				m_count++;
				return true;
			}
		};
	}
	inline void unlock() {
		if (m_count > 1) { m_count--; return; }
		m_count = 0; 
		m_tid = 0;
		int16AtomXchInc(*(volatile short *)&m_ticket);
	}
};

template <typename LockT = KKLock>
class KKHashLock {
private:
	LockT *m_locks;
	unsigned int m_total;
	KKHashLock(const KKHashLock &);
	KKHashLock &operator=(const KKHashLock&);
public:
	KKHashLock(unsigned int total) : m_total(total) {
		m_locks = new LockT[total];
	}
	~KKHashLock() {
		delete[] m_locks;
	}
	LockT &getLock(unsigned int hashVal) {
		return m_locks[hashVal % m_total];
	}
	LockT &operator [](unsigned int hashVal) {
		return m_locks[hashVal % m_total];
	}
	void lock() {
		for(int i = 0; i < m_total; i++) m_locks[i].lock();
	}
	void unlock() {
		for(int i = 0; i < m_total; i++) m_locks[i].unlock();
	}
	bool tryLock() {
		int i = 0;
		for(; i < m_total; i++) {
			if (!m_locks[i].tryLock()) break;
		}
		if (i == m_total) return true;
		for(; i >= 0; i--)
			m_locks[i].unlock();
		return false;
	}
};

class KKSemaphore
{
	HANDLE m_sem;
public:
	inline KKSemaphore(int initVal = 0, int maxVal = INT_MAX) {
		m_sem = ::CreateSemaphore( 
			NULL,           // default security attributes
			initVal,  // initial count
			maxVal,  // maximum count
			NULL);          // unnamed semaphore
	}
	inline ~KKSemaphore() 
	{
		if (m_sem) ::CloseHandle(m_sem);
	}
	inline HANDLE get() { return m_sem; }
	inline void wait() 
	{
		DWORD dwWaitResult = WaitForSingleObject( 
            m_sem,   // handle to semaphore
            INT_MAX); 
	}
	inline bool wait(unsigned int timeoutMilliSec) 
	{
		DWORD dwWaitResult = WaitForSingleObject( 
            m_sem,   // handle to semaphore
            timeoutMilliSec); 
		return (dwWaitResult == WAIT_OBJECT_0);
	}
	inline void signal(int deltaVal) 
	{
		if (!ReleaseSemaphore( 
            m_sem,  // handle to semaphore
            deltaVal,            // increase count by one
            NULL) )       // not interested in previous count
        {
           printf("ReleaseSemaphore error: %d\n", GetLastError());
        }
	}
};
