
#pragma once

#include "KKBase.h"
#include "KKList.h"
#include "KKObject.h"

template <typename Base = KKNul> struct KKTuple0 : Base {
	KKTuple0() { }
};

#define KKTuple_Init(N) m_p##N(p##N)

#define DECL_KKTuple(N) \
template <KKSufCList(typename P, typename Base = KKNul, N)> \
struct KKTuple##N : public Base { \
	KKDSCList(P, m_p, N);\
	KKTuple##N() { } \
	KKTuple##N(KKDCList(P,p,N)) : KKMCList(KKTuple_Init,N) { } \
}
	//template <typename T0> \
	//KKTuple##N(T0 t0, KKDCList##N(P,p)):Base(t0), \
	//           KKMCList##N(KKTuple_Init) { } \
	//template <typename T0, typename T1> \
	//KKTuple##N(T0 t0, T1 t1, KKDCList##N(P,p)):Base(t0, t1), \
	//           KKMCList##N(KKTuple_Init) { } \
	//template <typename T0, typename T1, typename T2> \
	//KKTuple##N(T0 t0, T1 t1, T2 t2, KKDCList##N(P,p)):Base(t0, t1, t2), \
	//           KKMCList##N(KKTuple_Init) { } \
	//template <typename T0, typename T1, typename T2, typename T3> \
	//KKTuple##N(T0 t0, T1 t1, T2 t2, T3 t3, KKDCList##N(P,p)):Base(t0, t1, t2, t3),\
	//           KKMCList##N(KKTuple_Init) { } \
//}

DECL_KKTuple(1);
DECL_KKTuple(2);
DECL_KKTuple(3);
DECL_KKTuple(4);
DECL_KKTuple(5);
DECL_KKTuple(6);
DECL_KKTuple(7);
DECL_KKTuple(8);
DECL_KKTuple(9);
DECL_KKTuple(10);
DECL_KKTuple(11);
DECL_KKTuple(12);
DECL_KKTuple(13);
DECL_KKTuple(14);
DECL_KKTuple(15);
DECL_KKTuple(16);

KKAssert(sizeof(KKTuple2<int,int,KKObject>) >= 2 * sizeof(int) + sizeof(KKObject));
KKAssert(sizeof(KKTuple2<int,int>) >= 2 * sizeof(int));
KKAssert(sizeof(((KKTuple2<float, double> *)0)->m_p1) == sizeof(float));
KKAssert(sizeof(((KKTuple2<float, double> *)0)->m_p2) == sizeof(double));
KKAssert(sizeof(((KKTuple4<float, int, double, int> *)0)->m_p3) == sizeof(double));

