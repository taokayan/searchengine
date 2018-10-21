// Multiplexer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "KKObject.h"
#include "KKLockGuard.h"
#include "KKFunctor.h"
#include "KKTask.h"
#include "KKVector.h"
#include "KKTuple.h"
#include "KKBox.h"
#include "KKThreadPool.h"
#include <vector>
#include <Windows.h>
#include <MMSystem.h>
#include <emmintrin.h>
#pragma comment(lib, "winmm.lib")

extern void testKKFunctor();
extern void testKKVector();
extern void testKKTask();
extern void testKKQueue();
extern void testKKQueue2();
extern void testKKAVLTree();
extern void testKKAVLTree2();
extern void testKKAVLTree3();
extern void testKKAVLTree4();
extern void testKKAVLTree5();
extern void testKKRBTree();
extern void testKKRBTree2();
extern void testKKRBTree3();
extern void testKKRBTree4();
extern void testKKRBTree5();
extern void testKKHeap();
extern void testKKHash();
extern void testKKHash2();
extern void testSIMD();
extern void testKKThreadPool();

struct A { int a; 
private :
	A(const A&);
};
struct B { int b; virtual ~B() {} };
struct C: public A, public B { int c; };
class D : public KKObject {
public:
	int d;
	D() { }
private:
	D(const D&);
};

#define FAIL printf("Failed at %s line %d\n", (const char *)__FILE__, (int)__LINE__)

void testKKTuple() {
	printf("Enter testKKTuple\n");
	KKTuple0<> t0;
	KKTuple1<int> t1;
	t1.m_p1 = 0;
	KKRef<KKTuple2<int *, float, KKObject> > t3 = new KKTuple2<int *, float, KKObject>;
	if (t3->refCount() != 1) printf("Failed!\n");
	t3->m_p2 = 1.3f;
}

DWORD testKKRef(KKTuple4<KKRef<D>, KKRef<D>, KKWeakRef<D>, KKWeakRef<D> > *p) 
{	
	int n = 100000;
	KKRef<D> a,b;
	KKWeakRef<D> c,d;
	a = new D();
	if (a.refCount() != 1) FAIL;
	if (a.weakRefCount() != 1) FAIL;
	c = a;
	if (a.refCount() != 1) FAIL;
	if (a.weakRefCount() != 2) FAIL;
	if (c.refCount() != 1) FAIL;
	if (c.weakRefCount() != 2) FAIL;
	b = c; // upgrade ok...
	if (b.refCount() != 2) FAIL;
	if (b.weakRefCount() != 3) FAIL;
	if (b.get() == 0) FAIL;
	if (c.refCount() != 2) FAIL;
	if (c.weakRefCount() != 3) FAIL;
	if (b.get() != a.get()) FAIL;
	a = 0;
	if (b.refCount() != 1) FAIL;
	if (b.weakRefCount() != 2) FAIL;
	if (b.get() == 0) FAIL;
	b = 0;
	b = c; // upgrade fails...
	if (c.refCount() != 0) FAIL;
	if (c.weakRefCount() != 1) FAIL;
	if (b.refCount() != 0) FAIL;
	if (b.get() != 0) FAIL;
	a = new D();
	b = a.xch(0);
	if (a.get() != 0) FAIL;
	if (b.get() == 0) FAIL;
	if (b.refCount() != 1) FAIL;
	a = new D();
	b = a.cmpXch(0, 0);
	if (a.get() == 0) FAIL;
	if (b.get() == 0) FAIL;
	if (a.refCount() != 2) FAIL;
	a = new D();
	b = a.cmpXch(new D(), a.get());
	if (a.get() == 0) FAIL;
	if (b.get() == 0) FAIL;
	if (a.refCount() != 1) FAIL;
	if (b.refCount() != 1) FAIL;

	// multithread testing...
	for(int i = 0; i < n; i++) {
		KKLocalRef<D> m;
		KKWeakRef<D> n;
		p->m_p1 = new D;
		p->m_p2 = p->m_p1;
		p->m_p2 = new D;
		p->m_p1 = p->m_p2;
		p->m_p3 = p->m_p1;
		p->m_p4 = p->m_p3;
		p->m_p2 = p->m_p4;
		m = p->m_p4;
		n = p->m_p1;
		if (p->m_p1.get() == (void *)1 ||
			p->m_p2.get() == (void *)1 ) printf("fuck!!! bad_ptr\n");
		p->m_p2 = 0;
		p->m_p1 = 0;
		p->m_p3.reset();
		p->m_p4.reset();
	}
	return 0;
}

void loop(int i, double d)
{
	while(1) {
		d += ::rand() / (++i);
		d *= ::rand() / (++i);
	}
}

void testKKRef() {
	const int nThread = 5;

	printf("sizeof(KKRef)=%d, sizeof(KKLocalRef)=%d\n", (int)sizeof(KKRef<D>), (int)sizeof(KKLocalRef<D>));

	for(int j = 1; j < nThread; j++) {
		KKTuple4<KKRef<D>, KKRef<D>, KKWeakRef<D>, KKWeakRef<D> > p, q;
		
		long long t = ::timeGetTime(), t2;
		KKRef<D>::m_waste = 0;
		{
			// parallel
			KKTask tasks[nThread];
			for(int i = 0; i < j; i++) tasks[i].set(&testKKRef, &p);
			for(int i = 0; i < j; i++) tasks[i].async();
			// auto join on KKTask destructor
		}
		t = ::timeGetTime() - t;

		t2 = ::timeGetTime();
		for(int i = 0; i < j; i++) {
			// single thread async...
			KKTask task;
			task.set(&testKKRef, &p);
			task.async();
		}
		
		t2 = ::timeGetTime() - t2;
		printf("testKKRef, nTh = %d, t = %d ms, efficiency = %.4lf\n", 
		   j, (int)t, KKRef<D>::m_waste, t2 / (double)t );
	}
}

void nanTest() {
	int N = 10000000;
	int sum1 = 0, sum2 = 0;
	DWORD t0,t1,t2;
	t0 = ::timeGetTime();
	::srand(0);
	for (int i = 0; i < N; i++) {
		volatile unsigned int r = 0x7ff00000u - (rand() & 0x1);
		volatile unsigned long long l = ((unsigned long long)r << 32) | r;
		sum1 += ::_isnan(*(double *)&l);
	}
	t1 = ::timeGetTime();
	::srand(0);
	for (int i = 0; i < N; i++) {
		volatile unsigned int r = 0x7ff00000u - (rand() & 0x1);
		volatile unsigned long long l = ((unsigned long long)r << 32) | r;
		// s111 1111 1111 mmmm ....
		if ((l & 0x7ff0000000000000ull) == 0x7ff0000000000000ull) {
			if (l & 0xfffffffffffffull) sum2++;
		}
	}
	t2 = ::timeGetTime();
	printf("nanTest N=%d, time1 = %dms sum1 = %d, time2 = %dms sum2 = %d\n", N, t1-t0, sum1, t2-t1, sum2);
}
int _tmain(int argc, _TCHAR* argv[])
{
	printf("delay some time...\n");
	::Sleep(2000);
	nanTest();
	testKKRef();
	testKKFunctor();
	testKKTask();
	testKKVector();
	testKKTuple();
	testKKQueue();
	testKKQueue2();
	testKKAVLTree();
	testKKAVLTree2();
	testKKAVLTree3();
	testKKAVLTree4();
	testKKAVLTree5();
	testKKRBTree();
	testKKRBTree2();
	testKKRBTree3();
	testKKRBTree4();
	testKKRBTree5();
	//testKKHeap();
	testKKHash();
	testKKHash2();
	testSIMD();
	testKKThreadPool();

	printf("test finish, start looping...\n");
	{
		KKTask t(&loop, 1, 1.0);
		KKTask t1(&loop, 1, 1.0);
		KKTask t2(&loop, 1, 1.0);
		KKTask t3(&loop, 1, 1.0);
		t.async();
		//t1.async();
		//t2.async();
		//t3.async();
	}
}
