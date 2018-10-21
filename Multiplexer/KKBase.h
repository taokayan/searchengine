
#pragma once

#define KKMakeName_(U, V) U##V
#define KKMakeName(U, V) KKMakeName_(U, V)
#define KKAssert_(F) \
	  typedef char KKMakeName(KKAssert_FAILED_, __LINE__)[(F) ? 1 : -1]; 

#define KKAssert(F) KKAssert_((F))

struct KKNul {};

//#define KKAssert(F) \
//	static_assert((F), "KKAssert_FAILED at " __FILE__ ) 

// KKSame
template <typename U, typename V> 
struct KKSame {
	enum _ {Val = false }; 
};
template <typename U> 
struct KKSame<U, U> { 
	enum _ {Val = true }; 
};

KKAssert((KKSame<int, int>::Val));
KKAssert((KKSame<int, long long>::Val == false));
KKAssert((KKSame<int, int *>::Val == false));

// KKHasIf
template <typename U, bool B> struct KKHasIf {};
template <typename U> struct KKHasIf<U, true> {
	typedef U T;
};


// KKIf
template <typename U, typename V, bool B> struct KKIf;

template <typename U, typename V>
struct KKIf<U,V,true> { typedef typename U T; };

template <typename U, typename V>
struct KKIf<U,V,false> { typedef typename V T; };

KKAssert(sizeof(KKIf<int, double, true>::T) == sizeof(int));
KKAssert(sizeof(KKIf<int, double, false>::T) == sizeof(double));


// KKLargest
template <typename U, typename V> 
struct KKLargest { typedef typename KKIf<U, V, (sizeof(U) >= sizeof(V))>::T T; };

KKAssert(sizeof(KKLargest<int, long long>::T) == sizeof(long long));
KKAssert(sizeof(KKLargest<double, float>::T) == sizeof(double));


// KKConverable
template <typename U, typename V>
struct KKConverable {
private:
	static float test(...);
	static double test(V);
	static U makeU();
public:
	enum _ { Val = (sizeof(test(makeU())) == sizeof(double)) };
};

KKAssert((KKConverable<int, const int>::Val));
KKAssert((KKConverable<const int, int>::Val));
KKAssert((KKConverable<int &, int>::Val));
KKAssert((KKConverable<volatile int, int>::Val));
KKAssert(!(KKConverable<long long, int *>::Val));


//
// Atomic definitions...
//
#define int16AtomXchInc(a)  _InterlockedIncrement16((short *)&(a))
#define int16AtomXchDec(a)  _InterlockedDecrement16((short *)&(a))
#define int16AtomCmpXch(a,b,c) _InterlockedCompareExchange16((volatile short *)&a,b,c);
#define int32AtomXchAdd(a,b)  InterlockedExchangeAdd((unsigned int*)&(a),(b))
#define int64AtomXchAdd(a,b)  InterlockedExchangeAdd64((volatile long long*)&(a),(b))
#define int32AtomXch(a,b)  InterlockedExchange((unsigned int*)&(a),(b))
#define int64AtomXch(a,b)  InterlockedExchange64((volatile long long*)(void *)(&(a)),(b))
#define int32AtomCmpXch(a,b,c) InterlockedCompareExchange((unsigned int*)&(a),(b),(c))
#define int64AtomCmpXch(a,b,c) InterlockedCompareExchange64((volatile long long*)&(a),(b),(c))

#if _WIN64 || __x86_64__ || __ppc64__
#define pointerAtomXch(a,b)  (void *)InterlockedExchange64((volatile long long *)&(a),(unsigned __int64)(b))
#define pointerAtomCmpXch(a,b,c)  (void *)InterlockedCompareExchange64((volatile long long *)&(a),(unsigned __int64)(b), (unsigned __int64)(c))
#else
#define pointerAtomXch(a,b)  (void *)InterlockedExchange((unsigned __int32*)&(a),(unsigned __int32)(b))
#define pointerAtomCmpXch(a,b,c)  (void *)InterlockedCompareExchange((unsigned __int32 *)&(a),(unsigned __int32)(b), (unsigned __int32)(c))
#endif

#define floatAtomXch(a,b) InterlockedExchange((unsigned __int32*)&(a),*(unsigned __int32 *)&(b))
#define floatAtomCmpXch(a,b,c) InterlockedCompareExchange((unsigned __int32*)&(a),*(unsigned __int32 *)&(b), *(unsigned __int32 *)&(c))
#define doubleAtomXch(a,b) InterlockedExchange64((unsigned __int64*)&(a),*(unsigned __int64 *)&(b))
#define doubleAtomCmpXch(a,b,c) InterlockedCompareExchange64((unsigned __int64*)&(a),*(unsigned __int64 *)&(b), *(unsigned __int64 *)&(c))
