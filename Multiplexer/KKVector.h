
#include "KKBase.h"
#include "KKObject.h"
#include "KKLock.h"
#include "KKLockGuard.h"

template <typename T, typename Base = KKNul, 
	      typename Lock = KKNoLock, typename SizeT = int>
class KKVector
{
public:
	static const SizeT MIN_SIZE = 4;

	class Iterator {
		KKLockGuard<Lock> m_guard;
		SizeT m_index;
		KKVector<T, Base, Lock> *m_ref;
		T m_empty;
	public:
		Iterator(KKVector<T, Base, Lock> &v): m_ref(v), m_index(0) {
			m_guard.lock(m_ref->m_lock);
		}
		operator bool() {
			return m_index >= 0 && m_index < m_ref->m_length;
		}
		T &operator *() {
			if (m_index < m_ref->m_length) return (*m_ref)[m_index];
			else return m_empty;
		}
		T &operator ++() { 
			m_index++;
			if (m_index < m_ref->m_length) return (*m_ref)[m_index];
			else return m_empty;
		}
		T &operator ++(int) { 
			SizeT index = m_index;
			m_index++;
			if (index < m_ref->m_length) return (*m_ref)[index];
			else return m_empty;
		}
		T &operator --() {
			m_index--;
			if (m_index >= 0) return (*m_ref)[m_index];
			else return m_empty;
		}
		T &operator --(int) {
			SizeT index = m_index;
			m_index--;
			if (index >= 0) return (*m_ref)[index];
			else return m_empty;
		}
	};

private:
	inline SizeT offset_(SizeT size) {
		return ((size - m_length) >> 1);
	}
public:
	inline KKVector() : m_capacity(0), m_length(0), m_data(0), m_offset(0) { }
	inline ~KKVector() { reset(); }
	inline void resize(SizeT size) {
		KKLockGuard<Lock> g(m_lock);
		T *data = new T[size];
		SizeT offset = offset_(size);
		for(SizeT i = 0; i < m_length; i++) {
			data[offset + i] = m_data[m_offset + i];
		}
		if (m_data) delete [] m_data;
		m_data = data;
		m_capacity = size;
		m_offset = offset;
	}
	inline void reset() {
		KKLockGuard<Lock> g(m_lock);
		if (m_data) delete [] m_data;
		m_data = 0;
		m_capacity = m_length = m_offset = 0;
	}
	inline void push_back(T v) {
		KKLockGuard<Lock> g(m_lock);
		if (m_offset + m_length >= m_capacity) {
			resize(m_capacity ? (m_capacity << 1) : MIN_SIZE);
		}
		m_data[m_offset + m_length] = v;
		m_length++;
	}
	inline T pop_back_no_resize() {
		KKLockGuard<Lock> g(m_lock);
		if (!m_length) return T();
		m_length--;
		return m_data[m_offset + m_length];
	}
	inline T pop_back() {
		KKLockGuard<Lock> g(m_lock);
		T t = pop_back_no_resize();
		if (m_length < (m_capacity >> 1) && (m_capacity >> 1) > MIN_SIZE) {
			resize(m_capacity >> 1);
		}
		return t;
	}
	inline void push_front(T v) {
		KKLockGuard<Lock> g(m_lock);
		if (m_offset == 0) {
			resize(m_capacity ? m_capacity * 2 : MIN_SIZE);
		}
		m_data[--m_offset] = v;
		m_length++;
	}
	inline T pop_front_no_resize() {
		KKLockGuard<Lock> g(m_lock);
		if (!m_length) return T();
		m_length--;
		return m_data[m_offset++];
	}
	inline T pop_front() {
		KKLockGuard<Lock> g(m_lock);
		T t = pop_front_no_resize();
		if (m_length < (m_capacity >> 1) && (m_capacity >> 1) > MIN_SIZE) {
			resize(m_capacity >> 1);
		}
		return t;
	}
	inline SizeT length() const { return m_length; }
	inline SizeT capacity() const { return m_capacity; }
	inline T &operator[](SizeT i) { return m_data[m_offset + i]; }
	inline const T &operator[](SizeT i) const { return m_data[m_offset + i]; }
	inline bool runtimeAssert() {
		if (m_capacity < m_length) return false;
		if (m_offset < 0) return false;
		if (m_capacity < 0) return false;
		if (m_length < 0) return false;
		if (!m_data) {
			if (m_length || m_capacity || m_offset) return false;
		} else {
			if (m_capacity <= 0) return false;
		}
		return true;
	}
private:
	Lock m_lock;
	T *m_data;
	SizeT m_capacity;
	SizeT m_length;
	SizeT m_offset;
};

