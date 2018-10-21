
#pragma once

#include "KKBase.h"
#include "KKList.h"
#include "KKObject.h"
#include <stdlib.h>

namespace {
	template <int N> struct KKEmptyBlock_ {
		char m_c[N];
		KKEmptyBlock_() { memset(m_c, 0, sizeof(m_c)); }
	};
	static KKEmptyBlock_<128> emptyBlock_;
};

#define KKFunctorGenerate(N) \
template <KKPreCList(typename R, typename P, N)> \
struct KKInvokable##N { \
	virtual R fn(KKPreCList(void *, P, N)) = 0; \
}; \
template <KKPreCList(typename R, typename P, N)> \
struct KKInvoker##N : public KKInvokable##N##<KKPreCList(R, P, N)> {\
	typedef R (*Fn)(KKCList(P, N));\
	Fn m_fn;\
	KKInvoker##N##(Fn fn) : m_fn(fn) { } \
	virtual R fn(KKPreDCList(void *_, P, p, N)) { return (*(Fn)m_fn)(KKCList(p, N)); } \
};\
template <typename R, KKPreCList(typename C, typename P, N)> \
struct KKCInvoker##N : public KKInvokable##N##<KKPreCList(R, P, N)> {\
	typedef R (C::*Fn)(KKCList(P, N));\
	Fn m_fn; \
	KKCInvoker##N##(Fn fn) : m_fn(fn) { } \
	virtual R fn(KKPreDCList(void *c, P, p, N)) { return (((C *)c)->*m_fn)(KKCList(p, N)); } \
};\
template <KKPreCList(typename R,typename P,N)> \
class KKFn##N## { \
	enum _ { InvokerSize = sizeof(KKCInvoker##N##<R,KKPreCList(KKObject,P,N)>) }; \
	void *m_obj; \
	char m_invoker[InvokerSize]; \
	bool m_isKKObject; \
	typedef R (*Fn)(KKCList(P,N));\
public:\
	inline KKFn##N##() { memset(this, 0, sizeof(*this)); m_isKKObject = false; } \
	inline ~KKFn##N##() { \
	  if (m_isKKObject) ((KKObject *)m_obj)->deRef(); \
	} \
	inline KKFn##N##(const KKFn##N## &v) { \
		memcpy(this, &v, sizeof(*this)); \
		if (m_isKKObject) ((KKObject *)m_obj)->addRef(); \
	} \
	inline KKFn##N## &operator=(const KKFn##N## &v) { \
		if (this == &v) return (*this); \
		if (m_isKKObject) ((KKObject *)m_obj)->deRef(); \
		memcpy(this, &v, sizeof(*this)); \
		if (m_isKKObject) ((KKObject *)m_obj)->addRef(); \
		return (*this); \
	} \
	inline bool operator ==(const KKFn##N## &v) { \
		if (this == &v) return true; \
		return memcmp(this, &v, sizeof(*this)) == 0; \
	} \
	inline bool operator <(const KKFn##N## &v) { \
		if (this == &v) return false; \
		return memcmp(this, &v, sizeof(*this)) < 0; \
	} \
	inline bool operator >(const KKFn##N## &v) { \
		if (this == &v) return false; \
		return memcmp(this, &v, sizeof(*this)) > 0; \
	} \
	inline bool operator <=(const KKFn##N## &v) { \
		if (this == &v) return true; \
		return memcmp(this, &v, sizeof(*this)) <= 0; \
	} \
	inline bool operator >=(const KKFn##N## &v) { \
		if (this == &v) return true; \
		return memcmp(this, &v, sizeof(*this)) >= 0; \
	} \
	inline void set(Fn fn) { \
		clean(); \
		new (m_invoker) KKInvoker##N##<KKPreCList(R,P,N)>(fn); \
	} \
	inline void clean() { \
		if (m_isKKObject) ((KKObject *)m_obj)->deRef(); \
		memset(this, 0, sizeof(*this)); \
    }\
protected:\
	template <typename C, bool isKKObject> inline void set_(C *c, R (C::*fn)(KKCList(P,N))) { \
		clean(); \
		new (m_invoker) KKCInvoker##N##<R, KKPreCList(C,P,N)>(fn);\
		m_obj = c; m_isKKObject = isKKObject; \
		if (m_isKKObject) ((KKObject *)m_obj)->addRef(); \
	}\
public:\
	template <typename C> inline void set(KKRef<C> &c, R (C::*fn)(KKCList(P,N))) {\
		set_<C, true>(c.get(), fn);\
	}\
	template <typename C> inline void set(C *c, R (C::*fn)(KKCList(P,N))) {\
		set_<C, KKConverable<C *, KKObject *>::Val>(c, fn); \
	}\
	inline R operator()(KKDCList(P,p,N)) {\
		return ((KKInvokable##N##<KKPreCList(R,P,N)> *)m_invoker)->fn(KKPreCList(m_obj,p,N));\
	}\
	operator bool() { return memcmp(this, &emptyBlock_, sizeof(*this)) != 0; }\
	bool operator !() { return !(bool)(*this); } \
}

KKFunctorGenerate(0);
KKFunctorGenerate(1);
KKFunctorGenerate(2);
KKFunctorGenerate(3);
KKFunctorGenerate(4);
KKFunctorGenerate(5);
KKFunctorGenerate(6);
KKFunctorGenerate(7);
KKFunctorGenerate(8);
KKFunctorGenerate(9);
KKFunctorGenerate(10);
KKFunctorGenerate(11);
KKFunctorGenerate(12);
KKFunctorGenerate(13);
KKFunctorGenerate(14);
KKFunctorGenerate(15);
KKFunctorGenerate(16);

KKAssert(sizeof(KKInvoker0<int>) > 0);
KKAssert(sizeof(KKCInvoker0<int, KKNul>) > 0);
KKAssert(sizeof(KKFn0<int>) > 0);
KKAssert(sizeof(KKFn0<int>) > 0);
KKAssert(sizeof(KKFn0<void>) == sizeof(KKFn2<int, int, int>));

