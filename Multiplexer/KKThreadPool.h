
#pragma once

#include "KKObject.h"
#include "KKQueue.hpp"
#include "KKTask.h"
#include "KKLock.h"
#include "KKAtomic.hpp"
#include "KKAVLTree.hpp"
#include "KKRBTree.h"

template <typename Base = KKNul>
class KKThreadPool : public Base
{
public:
	typedef DWORD TimeT;
	typedef KKAVLTree<TimeT, KKTask, KKNul, KKLock, KKObject> Tree;
	typedef typename Tree::Node TimeJobNode;
	typedef typename KKQueue2<KKTask> JobQueue;
private:
	KKThreadPool(const KKThreadPool&);
	KKThreadPool &operator =(const KKThreadPool&);
private:
	JobQueue m_jobs;
	Tree m_timeJobs;
	KKTask *m_threads;
	KKSemaphore *m_sems;

	int m_nThreads;
	KKAtomic<short> m_started;
	KKAtomic<short> m_stopSignal;

	static void run_(KKThreadPool *pool, int id) {
		long long runed = 0;
		while (true) {
			while (KKLocalRef<JobQueue::Node> node = pool->m_jobs.pop()) {
				try { node->get().run();  runed++; } catch (...) { }
			}
			TimeT dt = 0;
			while (KKLocalRef<TimeJobNode> firstTJob = pool->m_timeJobs.first()) {
				TimeT t = ::timeGetTime();
				if (firstTJob->key() > t) {
					dt = firstTJob->key() - t;
					break;
				}
				else if (pool->m_timeJobs.remove(firstTJob.get())) {
					try { firstTJob->val().run(); runed++; } catch (...) { }
				}
			}
            if (pool->m_stopSignal) break;
			if (dt == 0) {
				pool->m_sems[id].wait();
			} else {
				pool->m_sems[id].wait(dt);
			}
		}
		long long n = pool->m_timeJobs.count();
		printf("thread %d stopped, worked for %d jobs, %d jobs unfinished\n", 
			   (int)::GetCurrentThreadId(), (int)runed, int(n));
	}

public:
	KKThreadPool(int nThreads) : m_nThreads(nThreads) {
		m_threads = new KKTask[nThreads];
		m_sems = new KKSemaphore[nThreads];
	}
	~KKThreadPool() {
		m_stopSignal = 1;
		for(int i = 0; i < m_nThreads; i++) {
			m_sems[i].signal(1);
			m_threads[i].join();
		}
		delete[] m_threads;
		delete[] m_sems;
	}
	void start() {
		if (m_started.cmpXch(1, 0) == 0) {
			for(int i = 0; i < m_nThreads; i++) {
				m_threads[i].set(&KKThreadPool::run_, this, i);
				m_threads[i].async();
			}
		}
	}
	void stop(bool wait) {
		stopSignal = 1;
		if (wait) {
			for(int i = 0; i < m_nThreads; i++) {
				m_sems[i].signal(1);
				m_threads[i].join();
			}
		}
	}
	void add(KKTask &t) {
		m_jobs.push(t);
		for(int i = 0; i < m_nThreads; i++) {
			m_sems[i].signal(1);
		}
	}
	KKLocalRef<TimeJobNode> add(KKTask &t, TimeT time) {
		KKLocalRef<TimeJobNode> tjob = m_timeJobs.insert(time, t);
		for(int i = 0; i < m_nThreads; i++) {
			m_sems[i].signal(1);
		}
		return tjob;
	}
	bool remove(TimeJobNode *timeJob) {
		return m_timeJobs.remove(timeJob);
	}
};
