// ExternalSorter.cpp : Defines the entry point for the console application.
//
#pragma once

#include "stdafx.h"
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

#include <Windows.h>
#include "../../Multiplexer/KKTask.h"

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)

template <typename T, typename Base = KKNul>
class ExternalSortWriter : public Base {
public:
	class Job : public KKObject {
		friend class ExternalSortWriter;
		char   *m_data;
		size_t m_capacity;
		size_t m_len;
	public:
		Job(size_t capacity) : m_capacity(capacity) {
			m_data = (char *)::malloc(m_capacity);
			m_len = 0;
		}
		~Job() {
			if (m_data) ::free(m_data); 
		}
		bool full() {
			return m_len + sizeof(T) > m_capacity;
		}
		void push(const T& t) {
			if (m_len + sizeof(T) > m_capacity) return;
			*(T *)&(m_data[m_len]) = t;
			m_len += sizeof(T);
			return;
		}
	};
private:
	KKAtomic<Job*>  m_pendingJob;
	KKSemaphore     m_sem;
	FILE            *m_file;
	volatile bool   m_stop;
	volatile bool   m_stopped;
	KKTask          m_task;
public:
	void run_() {
		while (m_sem.wait(INT_MAX)) {
			Job *job = m_pendingJob.xch(0);
			if (job && m_file) {
				if (job->m_len/sizeof(T)) {
					::fwrite(job->m_data, sizeof(T), job->m_len/sizeof(T), m_file);
					::fflush(m_file);
				}
			}
			if (job) job->deRef();
			else if (m_stop) break;
		}
		m_stopped = true;
	}
public:
	ExternalSortWriter(FILE *file) : m_file(file), 
		m_pendingJob(0), m_stop(false), m_stopped(false) { 
		m_task.set(this, &ExternalSortWriter::run_);
		m_task.async();
	}
	void stop() {
		m_stop = true;
		m_sem.signal(1);
		while (!m_stopped) KKLock_Yield();
		Job *job = m_pendingJob.xch(0);
		if (job) job->deRef(); // safety
	}
	~ExternalSortWriter() {
		stop();
	}
	inline void push(Job *job) {
		if (!job || m_stop) return;
		job->addRef();
		while (m_pendingJob.cmpXch(job, 0) != 0) {
			KKLock_Yield();
		}
		m_sem.signal(1);
	}
};

// open the file only when reading, 
// avoid holding to much file descriptors
struct ExternalSort_ImmReader
{
	std::string  m_path;
	long long    m_offset;
	bool         m_finished, m_autoclose;
	FILE         *m_file;
	ExternalSort_ImmReader() : m_offset(0), m_finished(false), m_autoclose(false), m_file(0){ }
	~ExternalSort_ImmReader() { close(); }
	void close() {
		if (m_file) ::fclose(m_file);
		m_file = 0;
	}
	void init(const char *path, bool autoclose) {
		m_path = path;
		m_offset = 0;
		m_autoclose = autoclose;
		if (m_file) ::fclose(m_file);
		m_file = 0;
	}
	size_t fread(void *buf, size_t itemSize, size_t count) {
		if (m_offset < 0) return 0;
		if (!m_file) {
			m_file = ::fopen(m_path.c_str(), "rb");
			if (m_file) ::_fseeki64(m_file, m_offset, SEEK_SET);
		}
		if (!m_file) return 0;
		long long r = ::fread(buf, itemSize, count, m_file);
		if (r <= 0) m_offset = -1;
		else m_offset += r * itemSize;
		if (m_autoclose) {
			::fclose(m_file);
			m_file = 0;
		}
		return r;
	}
};

template <typename T, typename Base = KKNul, typename Lock = KKLock>
class ExternalSorter : public Base
{
public:
	enum _ { MaxThreads = 16,
		     WriteBufSize = 1048576 * 16
	       };
private:
	const size_t m_maxItemPerFile;
	std::string m_path;  // prefix of path
	int         m_fileIndex;
	short       m_nThreads;
	short       m_nFileSplit; // each file is splitted into multiple for storage saving
	long long   m_currentFileItemCount;
	FILE        *m_file; // current file
	FILE        *m_mergedFile; // output
	T           *m_items;
	
	std::vector<char> m_wBuf; // for flush
	Lock        m_lock;

	inline std::string getPath(int index, int splitIndex) {
		std::string p = m_path;
		char suffix[20];
		if (m_nFileSplit <= 1) sprintf(suffix, ".%d", index);
		else sprintf(suffix, ".%d%c", index, splitIndex + 'a');
		p += suffix;
		return p;
	}
	static void qsort_(T *data, long long len) {
		if (len <= 8) {
			if (len <= 2) {
				if (len == 2 && data[0] > data[1]) {
					T t = data[0]; data[0] = data[1]; data[1] = t;
				}
				return;
			}
			for (int i = 1; i < len; i++) {
				int j = 0;
				while (j < i && data[j] <= data[i]) j++;
				if (j < i) {
					T t = data[i];
					for (int k = i; k > j; k--) {
						data[k] = data[k-1];
					}
					data[j] = t;
				}
			}
			return;
		}
		long long i = 0, j = len - 1;
		T mid = RAND_MAX < len ? 
			    data[(((unsigned long long)rand() << 24) ^ (rand() << 12) ^ rand()) % len]:
				data[rand() % len];
		while (true) {
			while (i <= j && data[i] < mid) i++;
			while (i <= j && data[j] > mid) j--;
			if (i > j) {
				qsort_(data, j+1);
				qsort_(data+i, len-i);
				return;
			}
			T t = data[i]; data[i] = data[j]; data[j] = t;
			i++, j--;
		}
	}
public:
	void flush() {
		KKLockGuard<Lock> guard(m_lock);
		KKTask tasks[MaxThreads];
		long long starts[MaxThreads], ends[MaxThreads];
		if (!m_currentFileItemCount) return;
		if (m_file) ::fclose(m_file);
		m_file = ::fopen(getPath(m_fileIndex, 0).c_str(), "wb");
		if (!m_file) return;
		size_t wcount = 0, twcount = 0;
		size_t perfilemax = (m_currentFileItemCount / m_nFileSplit + 2);
		int splitIndex = 0;
		DWORD t0 = ::timeGetTime(), t1;
		typedef ExternalSortWriter<T, KKObject> Writer;
		KKLocalRef<Writer> writers = new Writer(m_file);
		KKLocalRef<Writer::Job> writerJob = 
			new Writer::Job(WriteBufSize);
		if (m_nThreads <= 1 || m_nThreads * m_nThreads >= m_currentFileItemCount) {
			qsort_(m_items, m_currentFileItemCount);
			t1 = ::timeGetTime();
			::fseek(m_file, 0, SEEK_SET);
			wcount = fwrite(m_items, sizeof(T), m_currentFileItemCount, m_file);
			twcount += wcount;
		} else {
			long long itemPerThread = m_currentFileItemCount / m_nThreads;
			long long lastNItem = m_currentFileItemCount - itemPerThread * (m_nThreads - 1);
			for (int i = 0; i < m_nThreads - 1; i++) {
				tasks[i].set(qsort_, &(m_items[i * itemPerThread]), itemPerThread);
				tasks[i].async();
			}
			qsort_(&(m_items[(m_nThreads-1) * itemPerThread]), lastNItem);
			for (int i = 0; i < m_nThreads; i++) {
				starts[i] = i * itemPerThread;
				ends[i] = starts[i] + (i == m_nThreads - 1 ? lastNItem : itemPerThread);
			}
			for (int i = 0; i < m_nThreads - 1; i++) tasks[i].join();
			t1 = ::timeGetTime();
			while (true) {
				int p = -1;
				for (int i = 0; i < m_nThreads; i++) {
					if (starts[i] >= ends[i]) continue;
					if (p < 0 || m_items[starts[i]] < m_items[starts[p]]) p = i;
				}
				if (p < 0) break;
				writerJob->push(m_items[starts[p]]);
				wcount++;
				starts[p]++;
				if (writerJob->full()) {
					writers->push(writerJob);
					writerJob = new Writer::Job(WriteBufSize);
				}
				if (wcount >= perfilemax) {
					writers->push(writerJob);
					writerJob = new Writer::Job(WriteBufSize);
					writers->stop();
					::fclose(m_file);
					m_file = ::fopen(getPath(m_fileIndex, ++splitIndex).c_str(), "wb");
					if (!m_file) return;
					twcount += wcount;
					wcount = 0;
					writers = new Writer(m_file);
				}
			}
			writers->push(writerJob);
			writers->stop();

			twcount += wcount;
		}
		if (m_file) ::fclose(m_file);
		m_file = 0;
		DWORD t2 = ::timeGetTime();
		printf("Exsort:%s_%d, nThreads %d, sortTime %dms, flushTime %dms, nflushed %llu\n", 
			   m_path.c_str(), m_fileIndex, m_nThreads, t1 - t0, t2 - t1, twcount);
		m_fileIndex++;
		m_currentFileItemCount = 0;
	}
	~ExternalSorter() {
		flush();
		if (m_items) delete[] m_items;
	}
	ExternalSorter(const char *path_, bool clear, int nThread = -1, 
		           size_t maxItemPerFile = 0x7fffffffull / sizeof(T), 
				   int nFileSplit = 8) : 
		m_maxItemPerFile(maxItemPerFile),
		m_path(path_), m_fileIndex(0), m_currentFileItemCount(0), m_file(0), m_mergedFile(0),
		m_nThreads(nThread), m_nFileSplit(nFileSplit) {

		if (m_nFileSplit < 2) m_nFileSplit = 2;
		if (m_nFileSplit > 26) m_nFileSplit = 26;
		if (m_nThreads <= 0) {
			SYSTEM_INFO sysinfo;
			::GetSystemInfo( &sysinfo );
			m_nThreads = sysinfo.dwNumberOfProcessors;
		}
		if (m_nThreads > MaxThreads) m_nThreads = MaxThreads;

		m_wBuf.resize(WriteBufSize, 0);
		int i = 0;
		FILE *file = 0;
		m_items = new T[m_maxItemPerFile];
		if (clear) {
			int i = 0; 
			bool stop;
			do {
				stop = true;
				for (int j = 0; j < m_nFileSplit; j++) {
					if (::remove(getPath(i, j).c_str()) == 0) stop = false;
				}
				i++;
			} while (!stop);
			return;
		}
		while (true) {
			file = ::fopen(getPath(i, 0).c_str(), "r+b");
			if (!file) { m_fileIndex = i; break; }
			fclose(file);
			i++;
		}
	}
	bool push(const T &item) {
		KKLockGuard<Lock> guard(m_lock);
		m_items[m_currentFileItemCount++] = item;
		if (m_currentFileItemCount >= (long long)m_maxItemPerFile) flush();
		return true;
	}
	int push(const T *items, size_t count) {
		if (!count) return 0;
		KKLockGuard<Lock> guard(m_lock);
		for (size_t i = 0; i < count; i++) {
			m_items[m_currentFileItemCount++] = items[i];
			if (m_currentFileItemCount >= (long long)m_maxItemPerFile) flush();
		}
		return count;
	}
	long long exSort(bool print = false, size_t maxBufItem = 0) {
		KKLockGuard<Lock> guard(m_lock);
		flush();
		if (m_fileIndex <= 0) return 0;
		m_mergedFile = fopen(m_path.c_str(), "wb");
		if (!m_mergedFile) return -1;

		std::vector<ExternalSort_ImmReader> files;
		files.resize(m_fileIndex + 1);

		if (maxBufItem < m_maxItemPerFile) maxBufItem = m_maxItemPerFile;
		size_t readAheadSize = maxBufItem / (m_fileIndex + 1);
		if (readAheadSize == 0 || maxBufItem > m_maxItemPerFile) {
			if (m_items) delete[] m_items;
			m_items = 0;
			if (readAheadSize == 0) readAheadSize = 1;
			m_items = new T[readAheadSize * (m_fileIndex + 1)];
		} else if (readAheadSize >= 0x3fffffff) readAheadSize = 0x3fffffff;

		struct DataIndexPair {
			T   m_data;
			int m_fileIndex;
		};
		DataIndexPair *heap = new DataIndexPair[m_fileIndex + 2];
		size_t heapLen = 0;

		std::vector<long long> readAheadLens;
		std::vector<long long> readAheadOffsets;
		readAheadLens.resize(m_fileIndex + 1, 0);
		readAheadOffsets.resize(m_fileIndex + 1, 0);
		
		// build heap
		std::vector<int> splitIndexes;
		splitIndexes.resize(m_fileIndex + 1, 0);
		int maxopenfiles = ::_getmaxstdio();
		for (int i = 0; i < m_fileIndex; i++) {
			files[i].init(getPath(i, 0).c_str(), i < (maxopenfiles - 20) ? false : true);
			readAheadLens[i] = files[i].fread(&(m_items[i * readAheadSize]), sizeof(T), readAheadSize);
			readAheadOffsets[i] = 0;
			if (readAheadLens[i] < 0) {
				readAheadLens[i] = 0;
				files[i].m_finished = true;
			}
			if (readAheadLens[i] > 0) {
				heapLen++;
				long long p = heapLen;
				heap[p].m_data = m_items[i * readAheadSize + (readAheadOffsets[i]++)];
				heap[p].m_fileIndex = i;
				while (p > 1) {
					if (heap[p/2].m_data > heap[p].m_data) {
						DataIndexPair t = heap[p]; heap[p] = heap[p/2]; heap[p/2] = t;
					}
					p /= 2;
				}
			}
		}

		// heap sort
		long long written = 0;
		bool valid = true;
		T lastWrite = heap[1].m_data;
		size_t wbufLen = 0;
		ExternalSortWriter<T>                   writer(m_mergedFile);
		KKLocalRef<ExternalSortWriter<T>::Job > writeJob = new ExternalSortWriter<T>::Job(WriteBufSize);
		DWORD t = ::timeGetTime();
		while (heapLen) {
			if (print) {
				if (heap[1].m_data < lastWrite) valid = false;
				lastWrite = heap[1].m_data;
			}
			if (writeJob->full()) {
				writer.push(writeJob);
				writeJob = new ExternalSortWriter<T>::Job(WriteBufSize);
			}
			writeJob->push(heap[1].m_data); written++;
			int fileInd = heap[1].m_fileIndex;
retry:
			if (readAheadOffsets[fileInd] >= readAheadLens[fileInd] && !files[fileInd].m_finished) {
				readAheadLens[fileInd] = files[fileInd].fread(&(m_items[fileInd * readAheadSize]), 
					                                          sizeof(T), readAheadSize);
				readAheadOffsets[fileInd] = 0;
				if (readAheadLens[fileInd] <= 0) {
					readAheadLens[fileInd] = 0;
					files[fileInd].close();
					::remove(getPath(fileInd, splitIndexes[fileInd]).c_str());
					if (++splitIndexes[fileInd] < m_nFileSplit) {
						files[fileInd].init(getPath(fileInd, splitIndexes[fileInd]).c_str(), files[fileInd].m_autoclose);
						goto retry;
					} else {
						files[fileInd].m_finished = true;
					}
				}
			}
			if (readAheadOffsets[fileInd] < readAheadLens[fileInd]) {
				heap[1].m_data = m_items[fileInd * readAheadSize + (readAheadOffsets[fileInd]++)];
			} else {
				if (heapLen <= 1) break;
				heap[1] = heap[heapLen--];
			}
			int p = 1;
			while (p*2 <= heapLen) {
				int next = p*2;
				if (next+1 <= heapLen && heap[next+1].m_data < heap[next].m_data) next++;
				if (heap[p].m_data < heap[next].m_data) break;
				DataIndexPair t = heap[p]; heap[p] = heap[next]; heap[next] = t;
				p = next;
			}
		}
		writer.push(writeJob);
		writer.stop();
		::fclose(m_mergedFile);
		if (m_items) delete[] m_items;
		m_items = 0;
		t = ::timeGetTime() - t;
		if (print) { 
			printf("Exsort %s %s, %lld items, time %dms\n", 
				   m_path.c_str(), valid ? "OK" : "failed", written, t); 
		}
		return written;
	}
};

inline int externalSortTest(int nThreads)
{
	typedef long long T;
	//long long N = 1048576ull * 512 / sizeof(T);
	long long N = 20000000;
	long long nItems = N * 8 + 1;
	struct Null { };
	ExternalSorter<long long> sorter("ex_sort_test_data", true, nThreads, N); 
	DWORD t0 = ::timeGetTime();
	for (long long i = 0; i < nItems; i++) {
		sorter.push(((T)rand() << 32) | ((T)rand() << 16) | (T)rand());
	}
	DWORD t1 = ::timeGetTime();
	printf("\ninsert time:%lldms, external sort begins...\n", t1 - t0);
	long long r = sorter.exSort(true, 0xffffffffull / sizeof(T));
	printf(" [%lld items] %s\n", r, (r == nItems ? "PASSED" : "FAILED!!!!!!"));
	return 0;
}

