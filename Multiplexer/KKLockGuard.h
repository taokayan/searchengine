
#pragma once

#include "KKLock.h"

template <typename Lock>
class KKLockGuard {
private:
	Lock *m_lock;
public:
	inline KKLockGuard() : m_lock(0) { }
	inline KKLockGuard(Lock &lock) : m_lock(&lock) {
		if (m_lock) m_lock->lock();
	}
	inline KKLockGuard(Lock &lock, bool &trySucceed) : m_lock(&lock) {
		if (m_lock->tryLock()) trySucceed = true;
		else {
			trySucceed = false;
			m_lock = 0;
		}
	}
	inline ~KKLockGuard() {
		unlock();
	}
	inline void lock(Lock &lock) {
		Lock *old = m_lock;
		m_lock = &lock;
		if (m_lock) m_lock->lock();
		if (old) old->unlock();
	}
	inline void unlock() {
		if (m_lock)  { m_lock->unlock(); m_lock = 0; }
	}
};
