
#pragma once

#include <memory>

#include <intrin.h>
#include <Windows.h>

#include "KKBase.h"
#include "KKAtomic.hpp"
#include "KKLock.h"
#include "KKLockGuard.h"

//#define KKObject_Lock KKTicketLock
#define KKObject_Lock            KKLock
#define KKObjectPreAllocCount    0

template <typename T> class KKRef;

class KKObject;

struct KKObjectCount_ {
#pragma pack(push)
#pragma pack(8)
	KKObject  *m_object;
	__int32   m_ref[2];
#pragma pack(pop)
};

#if KKObjectPreAllocCount > 0
template <size_t PreAllocSize>
class KKObjectCountHeap {
private:
	char              *m_start;
	char              *m_end;
	KKAtomic<char *>   m_head;
public:
	static KKObjectCountHeap m_instance;
    inline static KKObjectCountHeap *getInstance() { return &m_instance; }
private:
	KKObjectCountHeap(const KKObjectCountHeap &);
	KKObjectCountHeap &operator=(const KKObjectCountHeap &);
	inline char * &next(char *head) {
		return *(char **)head;
	}
private:
	inline KKObjectCountHeap() {
		size_t nitem = PreAllocSize;
		size_t itemsize = (sizeof(KKObjectCount_) + sizeof(void *) - 1) 
			              / sizeof(void *) * sizeof(void *);
		m_start = (char *)::malloc(itemsize * nitem);
		if (!m_start) return;
		m_end = m_start + itemsize * nitem;
		m_head = m_start;
		for (int i = 0; i < nitem - 1; i++) {
			*(char **)(m_head + i * itemsize) = (m_head + (i + 1)*itemsize);
		}
		*(char **)(m_head + (nitem - 1) * itemsize) = 0;
	}
public:
	inline void *alloc(size_t itemSize) {
retry:
		volatile char *head = m_head;
		if (!head) {
			return (void *)::malloc(itemSize);
		}
		volatile char *next = *(char **)head;
		if (m_head.cmpXch((char *)next, (char *)head) == head) {
			return (void *)head;
		}
		goto retry;
	}
	inline void free(void *p_) {
		char *p = (char *)p_;
		if (p >= m_start && p < m_end) {
retry:
			volatile char *head = m_head;
			*(char **)p = (char *)head;
			if (m_head.cmpXch((char *)p, (char *)head) != head) goto retry;
		} else {
			::free(p_);
		}
	}
};
template <size_t nItem>
KKObjectCountHeap<nItem>  KKObjectCountHeap<nItem>::m_instance;

#endif

class KKObjectCount : private KKObjectCount_ {
	template <typename T, typename Lock, bool IsStrong> friend class KKRef_;
	template <typename T> friend class KKRef;
	template <typename T> friend class KKLocalRef;
	template <typename T> friend class KKWeakRef;
	template <typename T> friend class KKLocalWeakRef;

	friend class KKObject;

#if KKObjectPreAllocCount > 0
	inline void *operator new(size_t size) {
		return KKObjectCountHeap<KKObjectPreAllocCount>::getInstance()->alloc(size);
	}
	inline void operator delete(void *p) {
		KKObjectCountHeap<KKObjectPreAllocCount>::getInstance()->free(p);
	}
#endif

	KKObjectCount(KKObject *obj)
		{ m_object = obj; m_ref[0] = m_ref[1] = 0; }

	// strong refCount
	inline int refCount() { return m_ref[0]; }

	// weakRefCount always >= refCount
	inline int weakRefCount() { return m_ref[1]; }

	template <bool IsStrong>
	inline void addRef() {
		if (IsStrong) int64AtomXchAdd(m_ref,  0x0000000100000001LL);
		else int64AtomXchAdd(m_ref, 0x0000000100000000LL);
	}

	template <bool IsStrong>
	inline void deRef()	 {
		volatile __int64 c;
		__int32 s, w;
		if (IsStrong) {
			// hold m_object because 'this' might be deleted when weakRef goes to 0
			volatile KKObject *obj = m_object;
			c = int64AtomXchAdd(m_ref, -0x0000000100000001LL);
			s = (__int32)c; 
			w = (c >> 32);
			if (s == 1) delete obj;
			else if (s < 1) {
#ifdef _DEBUG
				printf("error, strong refCount < 0 (%d)\n", s);
#endif
			}
		} else {
			c = int64AtomXchAdd(m_ref, -0x0000000100000000LL);
			w = (c >> 32);
		}
		if (w == 1) delete this;
		else if (w < 1) {
#ifdef _DEBUG
			printf("error, weak refCount < 0 (%d)\n", w);
#endif
		}
	}

	// KKWeakRef -> KKRef
	inline bool upgrade() {
		volatile __int64 r;
		__int32 s;
		int sc = 0;
		do {
			r = *(__int64 *)&m_ref[0];
			s = (__int32)(r); 
			if (s <= 0) {
#ifdef _DEBUG
				if (s < 0) printf("error, strongRef counter < 0 (%d) \n", s);
#endif
				return false;
			}
			if (int64AtomCmpXch(m_ref, r + 0x0000000100000001LL, r) == r) break;
		} while(1);
		return true;
	}
};

class KKObject {
	template <typename T, typename Lock, bool IsStrong> friend class KKRef_;
	template <typename T> friend class KKRef;
	template <typename T> friend class KKLocalRef;
	template <typename T> friend class KKWeakRef;
	template <typename T> friend class KKLocalWeakRef;
	template <typename T, typename U> friend struct KKConverable;
	friend class KKObjectCount;

private:
	KKObjectCount *m_kkobjCount;
	
private:
	KKObject operator =(const KKObject &);
	KKObject(const KKObject &);
public:
	virtual ~KKObject() { }

protected:
	KKObject() : m_kkobjCount(new KKObjectCount(this)) { }
public:
	inline int refCount() { return m_kkobjCount->refCount(); }
	inline int weakRefCount() { return m_kkobjCount->weakRefCount(); }
	inline void addRef() { m_kkobjCount->addRef<true>(); } 
	inline void deRef() { m_kkobjCount->deRef<true>(); } 
};

template <typename Lock> class KKRef__;
template <> class KKRef__<KKNoLock> {
protected:
#pragma pack(push)
#pragma pack(8)
	KKObjectCount *m_objCount;
#pragma pack(pop)
	inline KKNoLock &getLock() { return *(KKNoLock *)this; }
	inline KKNoLock &getLock() const { return *(KKNoLock *)this; }
};
template <> class KKRef__<KKLock> {
protected:
#pragma pack(push)
#pragma pack(8)
	KKObjectCount *m_objCount;
#pragma pack(pop)
	KKLock m_lock;
	inline KKLock &getLock() { return m_lock; }
	inline KKLock &getLock() const { return const_cast<KKLock &>(m_lock); }
};

template <typename T, typename Lock, bool IsStrong>
class KKRef_ : public KKRef__<Lock>
{
	KKAssert((KKConverable<T *, KKObject *>::Val));

	template <typename U, typename LockU, bool IsStrongU> 
	friend class KKRef_;

public: 
	static __int32 m_waste;

protected:

	template <typename U> 
	inline void set_(U *u) {
		if (u) u->m_kkobjCount->addRef<IsStrong>();
		KKObjectCount *old;		
		KKLockGuard<Lock> g(getLock());
		old = m_objCount;
		m_objCount = u ? u->m_kkobjCount : 0;
		if (old) old->deRef<IsStrong>();
	}

	inline T *get_() {
		if (!m_objCount) return 0;
		KKLockGuard<Lock> g(getLock());
		return m_objCount ? (T *)(m_objCount->m_object) : 0;
	}
	inline T *get_() const {
		if (!m_objCount) return 0;
		KKLockGuard<Lock> g(getLock());
		return m_objCount ? (T *)(m_objCount->m_object) : 0;
	}

public:
	inline KKRef_() { m_objCount = 0; }

	inline KKRef_(const KKRef_ &ref) {
		KKLockGuard<Lock> g((const_cast<KKRef_ &>(ref)).getLock());
		m_objCount = (const_cast<KKRef_ &>(ref)).m_objCount;
		if (m_objCount) m_objCount->addRef<IsStrong>();
		g.unlock();
		//if (m_objCount) m_objCount->addRef<IsStrong>();
	}
	template <typename U, typename LockU, bool IsStrongU>
	inline KKRef_(const KKRef_<U, LockU, IsStrongU> &ref_) {
		m_objCount = 0;
		assign(ref_);
	}
	inline ~KKRef_() {
		if (m_objCount) m_objCount->deRef<IsStrong>();
	}

	inline int refCount() {
		if (!m_objCount) return 0;
		KKLockGuard<Lock> g(getLock());
		return m_objCount ? m_objCount->refCount() : 0;
	}
	inline int weakRefCount() { 
		if (!m_objCount) return 0;
		KKLockGuard<Lock> g(getLock());
		return m_objCount ? m_objCount->weakRefCount() : 0;
	}

	void reset() {
		if (!m_objCount) return;
		KKObjectCount *c; 
		KKLockGuard<Lock> g(getLock());
		c = m_objCount;
		m_objCount = 0;
		g.unlock();
		if (c) c->deRef<IsStrong>();
	}

protected:
	template <typename U, typename LockU, bool IsStrongU>
	inline KKObjectCount *xch_(const KKRef_<U, LockU, IsStrongU> &ref_) {
		//typedef char noused_[(KKConverable<U, T>::Val) ? 1: -1];
		typedef KKRef_<U,LockU,IsStrongU> URef;
		KKLockGuard<Lock> g1;
		KKLockGuard<LockU> g2;
		URef *ref = &(const_cast<URef &>(ref_));
		if ((void *)this == (void *)ref) return 0;
		if ((void *)this > (void *)ref) {
			g2.lock(ref->getLock());
			g1.lock(getLock());
		} else {
			g1.lock(getLock());
			g2.lock(ref->getLock());
		}
		bool upgrade = (IsStrong && !IsStrongU);
		KKObjectCount *old = m_objCount;
		m_objCount = ref->m_objCount;
		if (m_objCount) {
			if (upgrade) {
				if (m_objCount->upgrade() == 0) m_objCount = 0;
			} else m_objCount->addRef<IsStrong>();
		}
		g2.unlock();
		g1.unlock();
		return old;
	}

	template <typename U, typename LockU, bool IsStrongU>
	inline KKRef_ & assign(const KKRef_<U, LockU, IsStrongU> &ref_) {
		typedef char assert_[KKConverable<U *, T *>::Val ? 1 : -1];
		if (!ref_.m_objCount) {
			reset(); 
		} else {
			KKObjectCount *old = xch_(ref_);
			if (old) old->deRef<IsStrong>();
		}
		return *this;
	}

public:
	KKRef_ &operator =(const KKRef_ &ref_) {
		return assign(ref_);
	}
	template <typename U, typename LockU, bool IsStrongU>
	KKRef_ &operator =(const KKRef_<U, LockU, IsStrongU> &ref_) {
		return assign(ref_);
	}
	inline KKRef_ xch(T *o) { // NOTE: o must guarantee not deleted. 
		KKAssert(IsStrong);
		KKLockGuard<Lock> g(getLock());
		KKRef_ old;
		old.m_objCount = m_objCount;
		if (o) {
			o->m_kkobjCount->addRef<IsStrong>();
			m_objCount = o->m_kkobjCount;
		} else m_objCount = 0;
		return old;
	}
	inline KKRef_ cmpXch(T *o, T *cmp) { // NOTE: o must guarantee not deleted. 
		KKAssert(IsStrong);
		KKLockGuard<Lock> g(getLock());
		if (get_() == cmp) return xch(o);
		else return *this;
	}
};

template <typename T, typename Lock, bool S>
int KKRef_<T, Lock, S>::m_waste = 0;

template <typename T>
class KKRef : public KKRef_<T, KKObject_Lock, true> {
	typedef KKRef_<T, KKObject_Lock, true> Super;
public:
	inline KKRef() { }
	inline KKRef(T *o) {
		if (o) { 
			o->m_kkobjCount->addRef<true>();
			m_objCount = o->m_kkobjCount;
		}
	}
	inline KKRef(const KKRef &ref) : Super(ref) { }
	template <typename U, typename LockU, bool IsStrong_>
	inline KKRef(const KKRef_<U, LockU, IsStrong_> &t) {
		m_objCount = 0;
		assign(t);
	}
	inline KKRef &operator =(const KKRef &t) {
		Super::operator=(t);
		return *this;
	}
	template <typename U, typename LockU, bool IsStrong_>
	inline KKRef &operator =(const KKRef_<U, LockU, IsStrong_> &t) {
		Super::operator=(t);
		return *this;
	}
	KKRef &operator =(T *o) {
		set_(o);
		return *this;
	}
	inline T *get() { return get_(); }
	inline const T *get() const { return get_(); }
	inline operator T*() { return get_(); }
	inline operator T*() const { return get_(); }
	inline T *operator->() { return get(); } // note: c->m_object might be deleted...
	inline operator bool() { return get() != 0; }
    inline KKRef xch(T *o) { // NOTE: o must guarantee not deleted. 
		KKLockGuard<KKObject_Lock> g(getLock());
		KKRef old;
		old.m_objCount = m_objCount;
		if (o) {
			o->m_kkobjCount->addRef<true>();
			m_objCount = o->m_kkobjCount;
		} else m_objCount = 0;
		return old;
	}
	inline KKRef cmpXch(T *o, T *cmp) { // NOTE: o must guarantee not deleted. 
		KKLockGuard<KKObject_Lock> g(getLock());
		if (get_() == cmp) return xch(o);
		else return *this;
	}
};

template <typename T>
class KKLocalRef : public KKRef_<T, KKNoLock, true> {
	typedef KKRef_<T, KKNoLock, true> Super;
public:
	inline KKLocalRef() { }

	template <typename U, typename LockU, bool IsStrong_>
	inline KKLocalRef(const KKRef_<U, LockU, IsStrong_> &t) {
		m_objCount = 0;
		assign(t);
	}
	inline KKLocalRef(T *o) {
		if (o) { 
			o->m_kkobjCount->addRef<true>();
			m_objCount = o->m_kkobjCount;
		}
	}
	inline KKLocalRef(const KKLocalRef &ref) : Super(ref) { }

	inline KKLocalRef &operator =(const KKLocalRef &t) {
		Super::operator=(t);
		return *this;
	}
	template <typename U, typename LockU, bool IsStrong_>
	inline KKLocalRef &operator =(const KKRef_<U, LockU, IsStrong_> &t) {
		Super::operator=(t);
		return *this;
	}
	KKLocalRef &operator =(T *o) {
		set_(o);
		return *this;
	}
	inline T *get() { return get_(); }
	inline const T *get() const { return get_(); }
	inline operator T*() { return get_(); }
	inline operator T*() const { return get_(); }
	inline T *operator->() { return get(); } // note: c->m_object might be deleted...
	inline operator bool() { return get() != 0; }
	inline KKLocalRef xch(T *o) { // NOTE: o must guarantee not deleted. 
		KKLocalRef old;
		old.m_objCount = m_objCount;
		if (o) {
			o->m_kkobjCount->addRef<true>();
			m_objCount = o->m_kkobjCount;
		} else m_objCount = 0;
		return old;
	}
	inline KKLocalRef cmpXch(T *o, T *cmp) { // NOTE: o must guarantee not deleted. 
		if (get_() == cmp) return xch(o);
		else return *this;
	}
};


template <typename T>
class KKWeakRef : public KKRef_<T, KKObject_Lock, false> {
	typedef KKRef_<T, KKObject_Lock, false> Super;
public:
	inline KKWeakRef() { }
	inline KKWeakRef(const KKWeakRef &ref) : Super(ref) { }
	inline KKWeakRef &operator =(const KKWeakRef &t) {
		Super::operator=(t);
		return *this;
	}
	template <typename U, typename LockU, bool IsStrong_>
	inline KKWeakRef(const KKRef_<U, LockU, IsStrong_> &t) {
		m_objCount = 0;
		assign(t);
	}
	template <typename U, typename LockU, bool IsStrong_>
	inline KKWeakRef &operator =(const KKRef_<U, LockU, IsStrong_> &t) {
		Super::operator=(t);
		return *this;
	}
};

