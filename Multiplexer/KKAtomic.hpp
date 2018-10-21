#pragma once

#include "KKBase.h"

template <typename T, int Size, long long Step>
class KKAtomic_;

template <typename T, long long Step>
class KKAtomic_<T, 2, Step>
{
#pragma pack(push, 2)
public:
	volatile T m_val;
	inline KKAtomic_(T t) : m_val(t) { }
	inline operator T() { return m_val; }
	inline operator T() const { return m_val; }
	inline KKAtomic_ &operator =(T t) { m_val = t; }
	inline T operator ++() { return (T)int16AtomXchInc(m_val) + 1; }
	inline T operator ++(int) { return (T)int16AtomXchInc(m_val); }
	inline T operator --() { return (T)(int16AtomXchDec(m_val) - 1); }
	inline T operator --(int) { return (T)int16AtomXchDec(m_val); }
	inline T cmpXch(T v, T c) { return (T)int16AtomCmpXch(m_val, v, c); }
#pragma pack(pop)
};

template <typename T, long long Step>
class KKAtomic_<T, 4, Step>
{
#pragma pack(push, 4)
public:
	volatile T m_val;
	inline KKAtomic_(T t) : m_val(t) { }
	inline operator T() { return m_val; }
	inline operator T() volatile { return m_val; }
	inline operator T() const { return m_val; }
	inline T operator ++() { return (T)(int32AtomXchAdd(m_val, Step) + Step); }
	inline T operator ++(int) { return (T)int32AtomXchAdd(m_val, Step); }
	inline T operator +=(T v) { return (T)(int32AtomXchAdd(m_val, (int)v*Step) + (int)v*Step); }
	inline T operator --() { return (T)(int32AtomXchAdd(m_val, -Step) - Step); }
	inline T operator --(int) { return (T)int32AtomXchAdd(m_val, -Step); }
	inline T operator -=(T v) { return (T)(int32AtomXchAdd(m_val, -(int)v*Step) - (int)v*Step); }
	inline T xch(T v) { return (T)int32AtomXch(m_val, v); }
	inline T cmpXch(T v, T c) { return (T)int32AtomCmpXch(m_val, v, c); }
	inline T cmpXch(T v, T c) volatile { return (T)int32AtomCmpXch(m_val, v, c); }
#pragma pack(pop)
};

template <typename T, long long Step>
class KKAtomic_<T, 8, Step>
{
#pragma pack(push, 8)
public:
	volatile T m_val;
	inline KKAtomic_(T t) : m_val(t) { }
	inline operator T() { return m_val; }
	inline operator T() const { return m_val; }
	inline operator T() volatile { return m_val; }
	inline T operator ++() { return (T)(int64AtomXchAdd(m_val, Step) + Step); }
	inline T operator ++(int) { return (T)int64AtomXchAdd(m_val, Step); }
	inline T operator +=(T v) { return (T)(int64AtomXchAdd(m_val, v*Step) + v*Step); }
	inline T operator --() { return (T)(int64AtomXchAdd(m_val, -Step) - Step); }
	inline T operator --(int) { return (T)int64AtomXchAdd(m_val, -Step); }
	inline T operator -=(T v) { return (T)(int64AtomXchAdd(m_val, -v*Step) - v*Step); }
	inline T xch(T v) { return (T)int64AtomXch(m_val, v); }
	inline T cmpXch(T v, T c) { return int64AtomCmpXch(m_val, v, c); }
	inline T cmpXch(T v, T c) volatile { return int64AtomCmpXch(m_val, v, c); }
#pragma pack(pop)
};

template <typename T>
class KKAtomic;

#define KKAtomic_Def(T) \
template <> \
class KKAtomic<T> : public KKAtomic_<T, sizeof(T), 1> { \
	typedef KKAtomic_<T, sizeof(T), 1> Base; \
public:\
	inline KKAtomic(T t = 0) : Base(t) { } \
	inline KKAtomic &operator =(T t) { m_val = t; return *this; } \
	inline volatile KKAtomic &operator =(T t) volatile { m_val = t; return *this; } \
};

KKAtomic_Def(short);
KKAtomic_Def(unsigned short);
KKAtomic_Def(int);
KKAtomic_Def(unsigned int);
KKAtomic_Def(long);
KKAtomic_Def(unsigned long);
KKAtomic_Def(long long);
KKAtomic_Def(unsigned long long);

template <typename T, int Size> class KKAtomicPtr_;
template <typename T>
class KKAtomicPtr_<T *, 4> : public KKAtomic_<T *, 4, sizeof(T)> {
	typedef KKAtomic_<T *, 4, sizeof(T)> Base;
public:
	inline KKAtomicPtr_(T *t = 0) : Base(t) { }
	inline T* xch(T* v) { return (T*)int32AtomXch(m_val, (int)v); }
	inline T* cmpXch(T* v, T* c) { 
		return (T*)int32AtomCmpXch(m_val, (int)v, (int)c); 
	}
};

template <typename T>
class KKAtomicPtr_<T *, 8> : public KKAtomic_<T *, 8, sizeof(T)> {
	typedef KKAtomic_<T *, 8, sizeof(T)> Base;
public:
	inline KKAtomicPtr_(T *t = 0) : Base(t) { }
	inline T* xch(T* v) { return (T*)int64AtomXch(m_val, (long long)v); }
	inline T* cmpXch(T* v, T* c) { 
		return (T*)int64AtomCmpXch(m_val, (long long)v, (long long)c); 
	}
};

template <typename T>
class KKAtomic<T*> : public KKAtomicPtr_<T*, sizeof(T*)> {
	typedef KKAtomicPtr_<T*, sizeof(T*)> Base;
public:
	inline KKAtomic(T *t = 0) : Base(t) { }
	inline operator T*() { return (T *)m_val; }
	inline operator T*() const { return (T *)m_val; }
	inline T* operator ->() { return (T *)m_val; }
};

KKAssert(sizeof(KKAtomic<short>) == 2);
KKAssert(sizeof(KKAtomic<int>) == 4);
KKAssert(sizeof(KKAtomic<unsigned long long>) == 8);
KKAssert(sizeof(KKAtomic<int *>) == sizeof(int *));

