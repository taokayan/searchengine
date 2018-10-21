

#include "stdafx.h"
#include "KKObject.h"
#include "KKLockGuard.h"
#include "KKFunctor.h"
#include "KKTask.h"
#include "KKQueue.hpp"
#include "KKAVLTree.hpp"
#include "KKRBTree.h"
#include "KKHeap.hpp"
#include "KKBox.h"
#include "KKThreadPool.h"
#include "KKHash.hpp"
#include <queue>
#include <map>
#include <unordered_map>

namespace {

	class A : public KKObject
	{
	public:
		static int m_checkCount;
	public:
		int m_v;
	public:
		virtual ~A() { 
			//printf("HIHI -%d\n", m_v); 
			m_checkCount--; 
		}
	public:
		A(int v): m_v(v) { 
			//printf("HIHI +%d\n", m_v); 
			m_checkCount++; 
		}
		void hihi0() { }
		int hihi(double f, int k) {
			//printf("A::hihi(%lf), v = %d\n", f , m_v + k);
			return 0;
		}
		void addTo(int *z, int x, int y) {
			*z = x + y;
		}
		void hihi2() {
			//printf("A::hihi2\n", m_v);
		}
	};

	int A::m_checkCount = 0;

	int b(double f, int a) {
		//printf("b() %lf\n", f);
		return 0;
	}

}

void testKKFunctor()
{
	printf("Enter " __FUNCTION__ "(), If no Assert failed, it passes\n");
	{
		KKRef<A> a(new A(1));
		//A *pa = a;
		//KKLockGuard g(pa);
		//a = new A(2);
		//KKLockGuard g2(pa);
		//g.unlock();
		//a = 0;
		KKFn2<int, double, int> fn, fn1;
		KKFn0<void> fn2, fn3;
		printf("size of Fn = %d\n", sizeof(fn));
		if (fn) printf("Assert failed...Functor bool()..\n");
		if (!!fn) printf("Assert failed...Functor !()..\n");
		fn.set(a, &A::hihi);
		fn1 = fn;
		fn1 = fn;
		fn1 = fn1;
		if (fn1 != fn) printf("Assert failed...Functor !=\n");
		if (!(bool)fn1) printf("Assert failed...Functor bool()\n");
		if (!fn1) printf("Assert failed...Functor !()\n");
		fn.clean();
		fn.clean();
		if (fn1 == fn) printf("Assert failed...Functor ==\n");
		fn2.set(a, &A::hihi2);
		fn2.set(a, &A::hihi2);
		a = a;
		a = 0;
		a = 0;
		fn1(0.2, 100);
		fn2();
		a = new A(456);
		if (a.get()->refCount() != 1) printf("refCount assert failed...\n");
		fn2.set(a, &A::hihi2);
		if (a.get()->refCount() != 2) printf("Assert failed, Fn not adding refCount\n");
		fn3.set(a.get(), &A::hihi2);
		if (a.get()->refCount() != 3) printf("Assert failed, Fn not adding refCount2\n");
		fn.set(&b);
		if (!(bool)fn1) printf("Assert failed...Functor bool()\n");
		if (!fn1) printf("Assert failed...Functor !()\n");
		fn(0.3, 101);
	}

	if (A::m_checkCount != 0) printf("Assert failed...checkCount\n");
}

void testKKTask()
{
	printf("Enter " __FUNCTION__ "(), If no Assert failed, it passes\n");
	{
		KKRef<A> a(new A(1));
		KKTask t(a, &A::hihi0);
		t.run();
		t.set(a.get(), &A::hihi, 0.1, 20);
		t.run();
		t.set(&b, 1.0, 4);
		t.run();
	}
			
	int x =0, y =0;
	{
		KKRef<A> a(new A(1));

		KKTask t(a, &A::addTo, &x, 10, 20);
		KKTask t2(a, &A::addTo, &y, 123, 400);
		t.async();
		t2.async();
		// t, t2 will join here...
	}
	if (x != 30 || y != 523) printf("Assert failed....KKTask::async\n");

	{
		int a = 10, b = 20, c = 0;
		KKTask t(KKTask::Lambda, [&]()->void {
			c = a + b;
		});
		t.async();
		t.join();
		if (c != 30) printf("Assert failed....KKTask::async lambda\n");
	}

	if (A::m_checkCount != 0) printf("Assert failed...checkCount\n");
}


template <typename Queue>
void testKKQueue_(const char *name)
{
	struct Test : public KKObject {
		int m_nPush;
		int m_pushChkSum;
		int m_nPop;
		int m_popChkSum;
		const char *m_name;
		Test(const char *name, int n) : 
			m_nPush(n), m_pushChkSum(0), m_nPop(0), m_popChkSum(0), m_name(name) { }
		void push(Queue *q) {
			for(int i = 0; i < m_nPush; i++) {
				int v = i;
				q->push(v);
				m_pushChkSum = m_pushChkSum * 0x9e3779b9 + v;
			}
			printf("%s push %d ints, chkSum = %d\n", 
				   m_name, m_nPush, m_pushChkSum);
		}
		void pop(Queue *q) {
			int m_maxQueueSize = 0;
			while (m_nPop < m_nPush) {
				int v;
				KKLocalRef<Queue::Node> n;
				int c = q->count();
				if (c > m_maxQueueSize) m_maxQueueSize = c;
				if (n = q->pop()) {
				  int v = n->get();
				  m_popChkSum = m_popChkSum * 0x9e3779b9 + v;
				  m_nPop++;
				}
			}
			::Sleep(10);
			if (q->pop()) printf("pop failed...\n");
			printf("%s pop %d ints, chkSum = %d, maxQueueSize = %d\n", 
				   m_name, m_nPop, m_popChkSum, m_maxQueueSize);
		}
	};

	struct Test2 : public KKObject {
		int m_nPush;
		int m_pushChkSum;
		int m_nPop;
		int m_popChkSum;
		KKLock m_lock;
		Test2(int n) : m_nPush(n), m_pushChkSum(0), m_nPop(0), m_popChkSum(0) { }
		void push(std::queue<int> *q) {
			for(int i = 0; i < m_nPush; i++) {
				int v = i;
				KKLockGuard<KKLock> g(m_lock);
				q->push(v);
				g.unlock();
				m_pushChkSum = m_pushChkSum * 0x9e3779b9 + v;
			}
			printf("std::queue push %d ints, chkSum = %d\n", m_nPush, m_pushChkSum);
		}
		void pop(std::queue<int> *q) {
			int m_maxQueueSize = 0;
			while (m_nPop < m_nPush) {
				int v;
				KKLockGuard<KKLock> g(m_lock);
				int c = q->size();
				if (c > m_maxQueueSize) m_maxQueueSize = c;
				if (c) {
					int v = q->front();
					q->pop();
					g.unlock();
					m_popChkSum = m_popChkSum * 0x9e3779b9 + v;
					m_nPop++;
				}
			}
			::Sleep(10);
			printf("std::queue pop %d ints, chkSum = %d, maxQueueSize = %d\n", m_nPop, m_popChkSum, m_maxQueueSize);
		}
	};

	int testSize = 10000000;
	{
		KKRef<Test> test(new Test(name, testSize));
		Queue q;
		KKTask t(test, &Test::push, &q);
		KKTask t2(test, &Test::pop, &q);
		DWORD time = ::timeGetTime();
		t.async();
		t2.async();
		t.join();
		t2.join();
		time = ::timeGetTime() - time;
		if (test->m_popChkSum != test->m_pushChkSum) {
			printf("%s chksum failed...\n", name);
		}
		printf("n=%d, %s time = %lld\n", testSize, name, (long long)time);
	}
	{
		KKRef<Test2> test(new Test2(testSize));
		std::queue<int> q;
		KKTask t(test, &Test2::push, &q);
		KKTask t2(test, &Test2::pop, &q);
		DWORD time = ::timeGetTime();
		t.async();
		t2.async();
		t.join();
		t2.join();
		time = ::timeGetTime() - time;
		printf("n=%d, std::queue time = %lld\n", testSize, (long long)time);
	}
}

void testKKQueue()
{
	//struct QueueID { };
	//struct Queue2ID { };
	//typedef KKQueue<int, KKNul, KKLock, KKFixHeapBase<QueueID, sizeof(KKQueue<int>::Node), 10000000> > QueueT;
	//typedef KKQueue2<int, KKNul, KKFixHeapBase<Queue2ID, sizeof(KKQueue2<int>::Node), 10000000> > Queue2T;
	printf("\nEnter " __FUNCTION__ "(), If no Assert failed, it passes\n");
	testKKQueue_<KKQueue<int, KKNul, KKLock> >("KKQueue");
	testKKQueue_<KKQueue2<int, KKNul> >("KKQueue2");
}

template <typename Queue> 
void testKKQueue2_(const char *name, int N)
{
	struct testKKQueue2_ {
		Queue          queue;
		int            m_total;
		KKAtomic<int>  m_toPushTotal;
		KKAtomic<int>  m_popTotal;
		KKAtomic<int>  m_sum1;
		KKAtomic<int>  m_sum2;
		void push() {
			while (m_toPushTotal-- > 0) {
				int r = rand();
				queue.push(r);
				m_sum1 += r;
			}
		}
		void pop() {
			while (m_total > m_popTotal) {
				KKLocalRef<Queue::Node> node = queue.pop();
				if (node) {
					m_sum2 += node->get();
				    m_popTotal++;
				}
			}
		}
	};
	testKKQueue2_ test;
	const int total = 10000000;
	const int Max = 16;
	if (N > Max) N = Max;
	KKTask s[Max], t[Max];
	test.m_toPushTotal = test.m_total = total;
	for (int i = 0; i < N; i++) {
		s[i].set(&test, &testKKQueue2_::push);
		t[i].set(&test, &testKKQueue2_::pop);
	}
	DWORD t0 = ::timeGetTime();
	for (int i = 0; i < N; i++) {
		s[i].async();
		t[i].async();
	}
	for (int i = 0; i < N; i++) {
		s[i].join();
		t[i].join();
	}
	if (test.m_sum1 == test.m_sum2)  {
		t0 = ::timeGetTime() - t0;
		printf("%s size = %d, nThreads = %d, time = %dms chksum = %u OK\n", 
			   name, (int)total, N*2, (int)t0, (int)test.m_sum1);
	}
	else printf("%s size = %d, nThreads = %d, (%d)!=(%d), FAILED!!!\n", 
		name, (int)total, N*2, (int)test.m_sum1, (int)test.m_sum2);
}

void testKKQueue2()
{
	//struct QueueID { };
	//typedef KKQueue<int, KKNul, KKLock, KKFixHeapBase<QueueID, sizeof(KKQueue<int>::Node), 10000000> > QueueT;
	//struct Queue2ID { };
	//typedef KKQueue2<int, KKNul, KKFixHeapBase<Queue2ID, sizeof(KKQueue2<int>::Node), 10000000> > Queue2T;
	printf("\nEnter " __FUNCTION__ "(), If no Assert failed, it passes\n");
	for (int i = 0; i < 8; i++) {
		testKKQueue2_<KKQueue<int, KKNul, KKLock> >("KKQueue", i+1);
		testKKQueue2_<KKQueue2<int, KKNul> >("KKQueue2", i+1);
	}
}

template <typename KKTree>
void testKKTree_(const char *name) 
{
	KKTree map;
	int n = 3000000, p = 1;
	int golden = 135183431;
	DWORD time = ::timeGetTime();
	for(int i = 0; i < n; i++) {
		map.findInsert(i * golden, i * p, true);
		KKTree::Node *node = map.find(i/4 * golden);
		if (!node || node->val() != i/4 * p) {
			printf("%s test failed...\n", name);
			return;
		}
	}
	time = ::timeGetTime() - time;
	printf("%s %s insert & find, n=%d, time = %lld", 
		   (const char*)__FUNCTION__, name, n, (long long)time);
	if (!map.validate()) {
		printf("%s validate failed...\n", name);
	}
	time = ::timeGetTime();
	std::map<int, int> map2;
	for(int i = 0; i < n; i++) {
		map2[i * golden] = i * p;
		if (map2[i/4 * golden] != i/4 * p) {
			printf("std::map test failed...\n");
			return;
		}
	}
	time = ::timeGetTime() - time;
	printf(" vs std::map time = %lld\n", (long long)time);
}

void testKKAVLTree() 
{
	testKKTree_<KKAVLTree<int, int, KKNul, KKNoLock> >("KKAVLTree");
}

void testKKRBTree()
{
	testKKTree_<KKRBTree<int, int, KKNul, KKNoLock> >("KKRBTree");
}

template <typename KKTree>
void testKKTree2_(const char *name) 
{
	struct A {
		static void insert(KKTree *t, int n) {
			for(int i = 0; i < n; i++) {
				int r = rand();
				t->insert(r, r * r);
			}
		}
		static void remove(KKTree *t, int n, int *maxsize) {
			*maxsize = 0;
			while (n) {
				int c = t->count();
				if (c > *maxsize) *maxsize = c;
				KKTree::NodeRef ref = t->first();
				if (ref) t->remove(ref),n--;
			}
		}
	};
	KKTree tree;
	int n = 1000000;
	int maxsize = 0;
	DWORD time = ::timeGetTime();
	KKTask t(&A::insert, &tree, n);
	KKTask t2(&A::remove, &tree, n, &maxsize);
	t.async();
	t2.run();
	t.join();
	time = ::timeGetTime() - time;
	printf("%s %s n=%d, insert & remove on 2 threads, maxtreesize = %d, time = %lld\n", 
		   __FUNCTION__, name,n, maxsize, (long long)time);
}

void testKKAVLTree2() {
	testKKTree2_<KKAVLTree<int, int> >("KKAVLTree");
}

void testKKRBTree2() {
	testKKTree2_<KKRBTree<int, int> >("KKRBTree");
}

template <typename KKTree>
void testKKTree3_(const char *name) 
{
	//typedef KKAVLTree<int, int, KKNul, KKNoLock> KKTree;
	KKTree map;
	int n = 5000;
	KKTree tree;
	std::map<int, int> m;
	int i, s;
	for (s = 0; s < 10; s++) {
		srand(s);
		for(i = 0; i < n; i++) {
			int k = rand() % n;
			KKTree::Node *node = tree.find(k);
			if (m.find(k) == m.end()) {
				if (node) {
					goto FAILED;
				}
				m[k] = k*k;
				tree.insert(k,k*k);
			}
			else {
				if (!node) {
					goto FAILED;
				}
				if (m[k] != node->val()) {
					goto FAILED;
				}
				m.erase(k);
				tree.remove(k);
			}
			if (!tree.validate()) {
				//tree.print();
				goto FAILED;
			}
		}
	}
	printf("%s %s insert/find/remove n=%d OK\n", (const char*)__FUNCTION__, name, n);
	return;
FAILED:
	printf("FAILED %s %s (hint=%d %d)\n", (const char*)__FUNCTION__, name, s, i);
	//tree.print();
}

void testKKRBTree3() {
	testKKTree3_<KKRBTree<int,int> >("KKRBTree");
}

void testKKAVLTree3() {
	testKKTree3_<KKAVLTree<int,int> >("KKAVLTree");
}

template <typename Tree>
void testKKTree4_(const char *name) {
	//typedef KKRBTree<int,int> Tree;
	Tree tree;
	int n = 4000000;
	int seed = 1;

	int count0 = 0, count1 = 0, count2 = 0, count3 = 0;
	srand(seed);
	DWORD t = ::timeGetTime();
	for(int i = 0; i < n; i++) {
		int r = rand() % n;
		Tree::Node *node = tree.find(r);
		if (node) tree.remove(node), count1++;
		else tree.insert(r, r), count0++;
	}
	t = ::timeGetTime() - t;
	printf("%s %s insert/find/delete n=%d, time=%dms ", __FUNCTION__, name, n, t); 

	srand(seed);
	std::map<int, int> m;
	t = ::timeGetTime();
	for(int i = 0; i < n; i++) {
		int r = rand() % n;
		std::map<int,int>::iterator it = m.find(r);
		if (it != m.end()) m.erase(it), count3++;
		else m[r] = r, count2++;
	}
	t = ::timeGetTime() - t;
	printf(" vs std::map time=%dms ", t);
	if (count0 == count2 && count1 == count3) printf(" OK\n");
	else printf("validate FAILED!!!!\n");
}

void testKKRBTree4() {
	testKKTree4_<KKRBTree<int,int,KKNul, KKNoLock> >("KKRBTree");
}

void testKKAVLTree4() {
	testKKTree4_<KKAVLTree<int,int,KKNul, KKNoLock> >("KKAVLTree");
}

template <typename Tree>
void testKKTree5_(const char *name) {
	//typedef KKRBTree<int,int> Tree;
	Tree tree;
	int n = 4000000;
	int seed = 1;

	int count0 = 0, count1 = 0, count2 = 0, count3 = 0;
	srand(seed);
	for(int i = 0; i < n; i++) {
		int r = rand() % n;
		Tree::Node *node = tree.find(r);
		if (node) count1++;
		else tree.insert(r, r), count0++;
	}
	DWORD t = ::timeGetTime();
	for(int i = 0; i < n; i++) {
		int r = rand() % n;
		Tree::Node *node = tree.find(r);
		if (node) count1++;
	}
	t = ::timeGetTime() - t;
	printf("%s %s find n=%d, time=%dms ", __FUNCTION__, name, n, t); 

	srand(seed);
	std::map<int, int> m;
		for(int i = 0; i < n; i++) {
		int r = rand() % n;
		std::map<int,int>::iterator it = m.find(r);
		if (it != m.end()) count3++;
		else m[r] = r, count2++;
	}
	t = ::timeGetTime();
	for(int i = 0; i < n; i++) {
		int r = rand() % n;
		std::map<int,int>::iterator it = m.find(r);
		if (it != m.end()) count3++;
	}
	t = ::timeGetTime() - t;
	printf(" vs std::map time=%dms ", t);
	if (count0 == count2 && count1 == count3) printf(" OK\n");
	else printf("validate FAILED!!!!\n");
}

void testKKRBTree5() {
	testKKTree5_<KKRBTree<int,int,KKNul, KKNoLock> >("KKRBTree");
}

void testKKAVLTree5() {
	testKKTree5_<KKAVLTree<int,int,KKNul, KKNoLock> >("KKAVLTree");
}

//void testKKHeap() {
//	KKHeap<sizeof(int), 256> h;
//	const int n = 1000000;
//	//unsigned golden = 618643;
//	int** data = new int*[n];
//	memset(data, 0, sizeof(int*) * n);
//	for(int i = 0; i < n * 10; i++) {
//		//unsigned p = i * golden % n;
//		unsigned p = rand() % n;
//		if (!data[p]) {
//			data[p] = (int *)h.alloc();
//			*data[p] = p * p;
//		}
//		p = rand() % n;
//		if (data[p]) {
//			if (*data[p] != p * p) {
//				printf("KKHeap test failed...\n");
//				break;
//			}
//			h.free(data[p]);
//			data[p] = 0;
//		}
//	}
//	for(int i = 0; i < n; i++) {
//		if (data[i]) h.free(data[i]);
//	}
//	delete[] data;
//}


void testKKHash() 
{
	KKAssert(sizeof(KKHash<int,int>::Node) == sizeof(void*)+8);

	const char *name = "KKHash";
	typedef KKHash<int, int> HashT_;
	typedef KKHash<int, int, KKNul, KKLock, KKHeapBase<sizeof(HashT_::Node)> > HashT;
	
	int n = 10000000, p = 1;
	int golden = 135183431;
	DWORD time;
	{
		HashT map;
		time = ::timeGetTime();
		for(int i = 0; i < n; i++) {
			map.findInsert(i * golden, i * p, true);
			HashT::Node *node = map.find(i/4 * golden);
			if (!node || node->m_val != i/4 * p) {
				printf("%s test failed...\n", name);
				node = map.find(i/4 * golden);
				return;
			}
		}
		time = ::timeGetTime() - time;
		printf("%s %s insert & find, n=%d, time = %lld", 
			   (const char*)__FUNCTION__, name, n, (long long)time);
		::Sleep(5000);
	}

	time = ::timeGetTime();
	std::unordered_map<int, int> map2;
	for(int i = 0; i < n; i++) {
		map2[i * golden] = i * p;
		if (map2[i/4 * golden] != i/4 * p) {
			printf("std::unordered_map test failed...\n");
			return;
		}
	}
	time = ::timeGetTime() - time;
	printf(" vs std::unordered_map time = %lld\n", (long long)time);
	::Sleep(5000);
}


void testKKHash2() {
	const char *name = "KKHash2";
	typedef KKHash<int,int> HashT;
	
	int n = 10000000;
	int seed = 1;

	int count0 = 0, count1 = 0, count2 = 0, count3 = 0;
	srand(seed);
	DWORD t;
	{
		HashT hash;
		t = ::timeGetTime();
		for(int i = 0; i < n; i++) {
			int r = rand() % n;
			HashT::Node *node = hash.find(r);
			if (node) hash.remove(node), count1++;
			else hash.insert(r, r), count0++;
		}
		t = ::timeGetTime() - t;
		printf("%s %s insert/find/delete n=%d, time=%dms ", __FUNCTION__, name, n, t); 
	}

	srand(seed);
	std::unordered_map<int, int> m;
	t = ::timeGetTime();
	for(int i = 0; i < n; i++) {
		int r = rand() % n;
		std::unordered_map<int,int>::iterator it = m.find(r);
		if (it != m.end()) m.erase(it), count3++;
		else m[r] = r, count2++;
	}
	t = ::timeGetTime() - t;
	printf(" vs std::unordered_map time=%dms ", t);
	if (count0 == count2 && count1 == count3) printf(" OK\n");
	else printf("validate FAILED!!!!\n");
}

void testSIMD() 
{
	KKSIMD256<double> a,b,one,sum;
	int n = 10000000;

	DWORD t = ::timeGetTime();
	sum = (double)0.0;
	one = 1.0;
	for(int i = 0; i < n; i++) {
		a = (double)1 / (i + 1.0f);
		b.set(1.0, i + 1.0, i * 2 + 1.0, i * 3 + 1.0);
		b = one / b;
		a = a + b;
		b = a - b;
		a = a * b;
		b = a / b;
		sum = sum + a + b;
		//a = 0;
		//b = 0;
		//_mm256_zeroall();
	}
	t = ::timeGetTime() - t;
	double sum2 = 0;
	for(int i = 0; i < 4; i++) sum2 += sum[i];
	printf("testSIMD n = %d, time = %.3lf, chksum=%lf\n", n, t * 0.001, sum2);
}

void testKKThreadPool()
{
	KKThreadPool<KKNul> threadPool(2);
	KKAtomic<long long> chksum0 = 0, chksum1 = 0, count = 0;
	struct A {
		static void t(DWORD time, long long data, KKAtomic<long long> *chksum,
			          KKAtomic<long long> *count) {
			if (::timeGetTime() < time) {
				printf("KKThreadPool failed... triggered earlier\n");
			}
			(*chksum) += data;
			(*count)++;
		}
	};
	threadPool.start();
	int N = 1000000;
	DWORD t = ::timeGetTime();
	for(int i = 0; i < N; i++) {
		long long data = rand();
		DWORD t2 = t + rand() % 1000;
		threadPool.add(KKTask(&A::t, t2, data, &chksum1, &count), t2);
		chksum0 += data;
	}
	while (count != N) ::Sleep(1);
	t = ::timeGetTime() - t;
	if (count != N) {
		printf("KKThreadPool failed... trigger count(%d) != N(%d)\n", (int)count, N);
	}
	if (chksum0 != chksum1) {
		printf("KKThreadPool failed... chksum not match!\n");
	} else {
		printf("KKThreadPool for %d jobs, time %lldms chksum OK\n", N, t);
	}
}
