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
#include "../../Multiplexer/KKQueue.hpp"

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)

//#define EXTERNAL_SORT_DEBUG 

template <typename T, typename Base = KKNul>
class ExternalSortWriter : public Base {
public:
	class Job : public KKObject {
		//friend class ExternalSortWriter;
		std::vector<char> m_data;
		size_t m_capacity;
		size_t m_len;
	public:
		Job(size_t capacity) : m_capacity(capacity) {
			m_data.resize(m_capacity);
			m_len = 0;
		}
		~Job() {
		}
		size_t len() const { return m_len; }
		const char* data() const { return m_data.data(); }
		bool full() {
			return m_len + sizeof(T) > m_capacity;
		}
		void push(const T& t) {
			if (m_len + sizeof(T) > m_capacity) return;
			*(T *)&(m_data[m_len]) = t;
			m_len += sizeof(T);
			return;
		}
		void cleardata() {
			std::vector<char> tmp(std::move(m_data));
			m_len = 0;
		}
	};
private:
	// KKAtomic<Job*>  m_pendingJob;
	typedef KKQueue<KKRef<Job> > JobQueueT;
	JobQueueT  m_pendingJobs;
	KKSemaphore     m_sem;
	FILE            *m_file;
	volatile bool   m_stop;
	volatile bool   m_stopped;
	KKTask          m_task;
	int             m_maxJobCount;

	struct Runnable : public KKObject {
		ExternalSortWriter *m_this; // prevent m_task addRef to ExternalSortWriter*
		Runnable(ExternalSortWriter* this_): m_this(this_) {}
		void run() {
			m_this->run_();
		}
	};
	friend struct Runnable;
private:
	void run_() {
		while (m_sem.wait(INT_MAX)) {
			do {
				KKLocalRef<JobQueueT::Node> node = m_pendingJobs.pop();
				KKLocalRef<Job> job;
				if (node) job = node->get();
				else break;

				if (job && m_file) {
					if (job->len() / sizeof(T)) {
						::fwrite(job->data(), sizeof(T), job->len() / sizeof(T), m_file);
						job->cleardata();
					}
				}
				else break;
			} while (true);
			if (m_stop) break;
		}
		if (m_file) ::fclose(m_file);
		m_stopped = true;
	}
public:
	ExternalSortWriter(const ExternalSortWriter&) = delete;
	ExternalSortWriter& operator=(const ExternalSortWriter&) = delete;

	ExternalSortWriter(const char *file_path, const char *fmode, int maxJobCount_ = 32) : m_file(nullptr),
		m_stop(false), m_stopped(false), m_maxJobCount(maxJobCount_ <= 1 ? 1 : maxJobCount_) {
		m_file = ::fopen(file_path, fmode);
		m_task.set(new Runnable(this), &Runnable::run);
		m_task.async();
#ifdef EXTERNAL_SORT_DEBUG
		printf("ExternalSortWriter::ctor()\n");
#endif
	}
	bool isFileOpened() const {
		return (bool)m_file;
	}
	FILE* getFile() {
		return m_file;
	}
	void stop() {
		if (m_stopped) {
			return;
		}
		m_stop = true;
		m_sem.signal(1);
		while (!m_stopped) ::Sleep(1);
	}
	~ExternalSortWriter() {
#ifdef EXTERNAL_SORT_DEBUG
		printf("ExternalSortWriter::dtor()\n");
#endif
		stop();
	}
	inline void push(Job* job_) {
		KKRef<Job> job = job_;
		if (!job || m_stop) return;
		while (m_pendingJobs.count() >= m_maxJobCount) {
			::Sleep(1);
		}
		m_pendingJobs.push(job);
		m_sem.signal(1);
	}
};

// open the file only when reading, 
// avoid holding to much file descriptors

// async read-ahead if possible
struct ExternalSort_ImmReader
{
public:
	bool         m_finished, m_autoclose;

private:
	std::string  m_path;
	long long    m_offset;
	
	FILE* m_file;

	enum _ { MaxAsyncReadRAM = 2 * 1024 * 1024 };
	KKTask            m_readthread;
	std::vector<char> m_buf;
	size_t            m_itemSize;
	size_t            m_maxitem_async;
	KKAtomic<size_t>  m_bufcount;
	KKAtomic<int>     m_stop;
	KKAtomic<int>     m_nowait;

	struct Runnable : public KKObject {
		ExternalSort_ImmReader* m_this; // prevent m_readthread addRef to ExternalSort_ImmReader*
		Runnable(ExternalSort_ImmReader* this_) : m_this(this_) {}
		void run() {
			m_this->_read_thread();
		}
	};
	friend struct Runnable;

	void _read_thread() {
		if (!m_file) {
			m_bufcount.xch((size_t)-1); // mark it as "reach the end"
			return;
		}
		while (!(m_stop.cmpXch(0, 0))) {
			if (m_bufcount.cmpXch(0, 0) == 0) {
				long long r = ::fread(m_buf.data(), m_itemSize, m_maxitem_async, m_file);
				if (r <= 0) {
					m_bufcount.xch((size_t)-1);
					return; // reach the end
				}
				else {
					m_bufcount.xch(r);
					::Sleep(10);
				}
			}
			else {
				if (m_nowait.cmpXch(0, 0) == 0) ::Sleep(1);
			}
		}
	}

public:
	ExternalSort_ImmReader() : m_offset(0), m_finished(false), m_autoclose(false), m_file(0) {}
	~ExternalSort_ImmReader() {
		close();
	}
	void close() {
		m_stop = true;
		if (m_maxitem_async) m_readthread.reset();
		if (m_file) ::fclose(m_file);
		m_file = 0;
		m_bufcount.xch((size_t)-1);
		m_nowait.xch(0);
	}
	void init(const char* path, bool autoclose, size_t itemSize, size_t maxreadAheadCount) {
		close();

		m_path = path;
		m_offset = 0;
		m_autoclose = autoclose;

		m_itemSize = itemSize;
		m_maxitem_async = maxreadAheadCount;
		if (m_itemSize * m_maxitem_async > MaxAsyncReadRAM) {
			m_maxitem_async = MaxAsyncReadRAM / m_itemSize;
		}
		if (m_autoclose) m_maxitem_async = 0;
		m_buf.resize(m_itemSize * m_maxitem_async);

		if (m_maxitem_async) {
			m_file = ::fopen(m_path.c_str(), "rb");
			m_stop = false;
			m_bufcount.xch(0);
			m_readthread.set(new Runnable(this), &Runnable::run);
			m_readthread.async();
		}
	}
	size_t fread(void* buf, size_t itemSize, size_t count) {
		if (m_maxitem_async) {
_again:
			size_t bufCount = m_bufcount.cmpXch(0, 0);
			if (bufCount == 0) {
				m_nowait.xch(true);
				::Sleep(1);
				goto _again;
			}
			m_nowait.xch(false);
			if (bufCount == (size_t)-1) {
				return 0;
			}
			::memcpy(buf, m_buf.data(), bufCount * itemSize);
			m_bufcount.xch(0);
			return bufCount;
		}
		else {
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
	}
};

template <typename T, typename Base = KKNul, typename Lock = KKLock, 
	      bool DictOrder = true /* whether first 2 bytes in T are the most significant bytes and sorted in dictionary order */>
class ExternalSorter : public Base
{
public:
	enum _ {
		MaxThreads = (DictOrder ? 14 : 6), // sorting threads
		WriteBufSize = 1048576 * 16,
		MaxActiveWriter = 16
	       };
private:
	const size_t m_maxItemPerFile;
	std::string  m_path;  // prefix of path
	std::vector<std::string> m_temp_dirs; // prefix of intermediate sorted file path
	int         m_fileIndex;
	int         m_nFileSplit; // each file is splitted into multiple for storage saving
	long long   m_currentFileItemCount;
	size_t      m_totalItemCount;
	//FILE        *m_file; // current file
	FILE        *m_mergedFile; // output
	T           *m_items;
	T           *m_items2;

	int         m_nThreads;
	std::vector<char> m_wBuf; // for flush
	Lock        m_lock;

	typedef ExternalSortWriter<T, KKObject> Writer;
	std::vector<KKLocalRef<Writer> > m_active_writers;

	// get the path of an itermediate sorted file
	inline std::string getPath(int index, int splitIndex) {
		std::string p;
		if (m_temp_dirs.size() && m_path.length() > 0) {
			int last_name_begin = (int)m_path.length() - 1;
			while (last_name_begin > 0 && m_path[last_name_begin - 1] != '\\'
				&& m_path[last_name_begin - 1] != '/') {
				last_name_begin--;
			}
			p = m_temp_dirs[(index + splitIndex) % m_temp_dirs.size()];
			p += '\\';
			while (last_name_begin < m_path.length()) {
				p += m_path[last_name_begin++];
			}
		}
		else {
			p = m_path;
		}
		char suffix[20];
		if (m_nFileSplit <= 1) sprintf(suffix, ".%d", index);
		else sprintf(suffix, ".%d%c", index, splitIndex + 'a');
		p += suffix;
		return p;
	}

	static void qsort_(T *data, long long len) {
		if (len <= 8) {
			if (len <= 2) {
				if (len == 2 && data[1] < data[0]) {
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
			while (i <= j && mid < data[j]) j--;
			if (i > j) {
				qsort_(data, j+1);
				qsort_(data+i, len-i);
				return;
			}
			T t = data[i]; data[i] = data[j]; data[j] = t;
			i++, j--;
		}
	}

	struct DictSortParam {
		enum _ { BucketSize = 256 };
		T* m_data;
		uint64_t           m_itemOffsets[BucketSize];
		KKAtomic<uint64_t> m_remainItems[BucketSize];
		KKAtomic<uint64_t> m_totalSortedCount;
	};
	static void dictsortthread_(DictSortParam* param) {
		for (int i = 0; i < DictSortParam::BucketSize; i++) {
			uint64_t n = param->m_remainItems[i].xch(0);
			if (n) {
				qsort_(&(param->m_data[param->m_itemOffsets[i]]), n);
				param->m_totalSortedCount += n;
			}
		}
	}
public:
	void initiate_flush() {
		KKLockGuard<Lock> guard(m_lock);
		KKTask tasks[MaxThreads];

		long long starts[MaxThreads], ends[MaxThreads];
		if (!m_currentFileItemCount) return;

		size_t wcount = 0, twcount = 0;
		size_t perfilemax = (m_currentFileItemCount / m_nFileSplit + 2);
		int splitIndex = 0;
		DWORD t = ::GetTickCount();

		KKLocalRef<Writer> writer = new Writer(getPath(m_fileIndex, 0).c_str(), "wb");
		if (!writer->isFileOpened()) {
			printf("Exsorter: FAILED to open %s for writing!!!\n", getPath(m_fileIndex, 0).c_str());
			return;
		}

		KKLocalRef<Writer::Job> writerJob = 
			new Writer::Job(WriteBufSize);

		std::vector<KKLocalRef<Writer> > all_writers;
		all_writers.push_back(writer);

		if (DictOrder) { // radix sort for the first 2 chars, parallel qsort for the rest
			size_t counts[DictSortParam::BucketSize] = {};
			for (long long i = 0; i < m_currentFileItemCount; i++) {
				uint8_t c0 = *(uint8_t *)&(m_items[i]);
				uint32_t bucket = c0;
				counts[bucket]++;
			}
			DictSortParam param;
			param.m_data = m_items2;
			size_t offsets = 0;
			for (int c = 0; c < DictSortParam::BucketSize; c++) {
				param.m_remainItems[c] = counts[c];
				param.m_itemOffsets[c] = offsets;
				offsets += counts[c];
				counts[c] = 0;
			}
			for (long long i = 0; i < m_currentFileItemCount; i++) {
				uint8_t c0 = *(uint8_t*)&(m_items[i]);
				uint32_t bucket = c0;
				param.m_data[param.m_itemOffsets[bucket] + counts[bucket]] = m_items[i];
				counts[bucket]++;
			}
			for (int i = 0; i < m_nThreads; i++) {
				tasks[i].set(dictsortthread_, &param);
				tasks[i].async();
			}
			for (int i = 0; i < m_nThreads; i++) {
				tasks[i].join();
			}

			for (long long i = 0; i < m_currentFileItemCount; i++) {
				writerJob->push(m_items2[i]);
				wcount++;
				if (writerJob->full()) {
					writer->push(writerJob);
					writerJob = new Writer::Job(WriteBufSize);
				}
				if (wcount >= perfilemax) {
					writer->push(writerJob);
					writerJob = new Writer::Job(WriteBufSize);

					twcount += wcount;
					wcount = 0;

					writer = new Writer(getPath(m_fileIndex, ++splitIndex).c_str(), "wb");
					if (!writer->isFileOpened()) return;

					all_writers.push_back(writer);
				}
			}
			writer->push(writerJob);
			twcount += wcount;
		} else if (m_nThreads <= 1 || m_nThreads * m_nThreads >= m_currentFileItemCount) {
			qsort_(m_items, m_currentFileItemCount);
			::fseek(writer->getFile(), 0, SEEK_SET);
			wcount = fwrite(m_items, sizeof(T), m_currentFileItemCount, writer->getFile());
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

			// join all sorting threads
			for (int i = 0; i < m_nThreads - 1; i++) {
				tasks[i].join();
			}

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
					writer->push(writerJob);
					writerJob = new Writer::Job(WriteBufSize);
				}
				if (wcount >= perfilemax) {
					writer->push(writerJob);
					writerJob = new Writer::Job(WriteBufSize);

					twcount += wcount;
					wcount = 0;

					writer = new Writer(getPath(m_fileIndex, ++splitIndex).c_str(), "wb");
					if (!writer->isFileOpened()) return;
					all_writers.push_back(writer);
				}
			}
			writer->push(writerJob);
			twcount += wcount;
		}

		// wait for all previous active writers....
		size_t writerCount = m_active_writers.size() + all_writers.size();
		if (writerCount > MaxActiveWriter) {
			for (size_t i = 0; i < writerCount - MaxActiveWriter; i++) {
				m_active_writers[i]->stop();
				m_active_writers[i].reset();
			}
		}
		std::vector<KKLocalRef<Writer> > new_writer_list;
		for (size_t i = 0; i < m_active_writers.size(); i++) {
			if (m_active_writers[i]) new_writer_list.push_back(m_active_writers[i]);
		}
		for (size_t i = 0; i < all_writers.size(); i++) {
			new_writer_list.push_back(all_writers[i]);
		}
		m_active_writers = new_writer_list;

		t = ::GetTickCount() - t;
		printf("Exsort:flush %s_%d, nThreads %d, sortTime %ums, writeCount %llu, active_writers %llu\n", 
			   m_path.c_str(), m_fileIndex, m_nThreads, t, twcount, m_active_writers.size());
		m_fileIndex++;
		m_currentFileItemCount = 0;
	}

	void flush() {
		initiate_flush();

		KKLockGuard<Lock> guard(m_lock);
		// stop all previous active writers....
		for (int i = 0; i < m_active_writers.size(); i++) {
			m_active_writers[i]->stop();
		}
		m_active_writers.clear();
	}

	~ExternalSorter() {
		flush();
		if (m_items) delete[] m_items;
		if (m_items2) delete[] m_items2;
	}
	ExternalSorter(const ExternalSorter&) = delete;
	ExternalSorter& operator= (const ExternalSorter&) = delete;

	ExternalSorter(const char* path_, const std::vector<std::string>& temp_dirs, bool clear, int nThread = -1,
		size_t maxItemPerFile = 0x7fffffffull / sizeof(T),
		int nFileSplit = 8) :
		m_maxItemPerFile(maxItemPerFile),
		m_path(path_),
		m_temp_dirs(temp_dirs),
		m_fileIndex(0), 
		m_currentFileItemCount(0), 
		m_totalItemCount(0), 
		m_mergedFile(0),
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
		m_items2 = nullptr;
		if (DictOrder) {
			m_items2 = new T[m_maxItemPerFile];
		}
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
	size_t push(const T *items, size_t count) {
		if (!count) return 0;
		KKLockGuard<Lock> guard(m_lock);
		for (size_t i = 0; i < count; i++) {
			m_items[m_currentFileItemCount++] = items[i];
			m_totalItemCount++;
			if (m_currentFileItemCount >= (long long)m_maxItemPerFile) {
				initiate_flush();
			}
		}
		return count;
	}
	bool push(const T& item) {
		return push(&item, 1) == 1;
	}
	size_t fileCount() const { // return current number of sorted files
		return m_fileIndex + (m_currentFileItemCount > 0 ? 1 : 0);
	}
	size_t totalItems() const {
		return m_totalItemCount;
	}
	long long exSort(bool print = false, size_t maxBufItem = 0) {
		KKLockGuard<Lock> guard(m_lock);
		flush();
		if (m_fileIndex <= 0) return 0;

		std::vector<ExternalSort_ImmReader> files;
		files.resize(m_fileIndex + 1);

		if (m_items) delete[] m_items;
		if (m_items2) delete[] m_items2;
		m_items = m_items2 = 0;

		if (maxBufItem < m_maxItemPerFile) maxBufItem = m_maxItemPerFile;
		size_t readAheadSize = maxBufItem / (m_fileIndex + 1);
		if (readAheadSize == 0 || maxBufItem > m_maxItemPerFile) {
			if (readAheadSize == 0) readAheadSize = 1;
		} else if (readAheadSize >= 0x3fffffff) readAheadSize = 0x3fffffff;

		m_items = new T[readAheadSize * (m_fileIndex + 1) + 10];

		struct DataIndexPair {
			T   m_data;
			int m_fileIndex;
		};
		//DataIndexPair *heap = new DataIndexPair[m_fileIndex + 2];
		std::vector<DataIndexPair> heap;
		heap.resize(m_fileIndex + 2);
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
			files[i].init(getPath(i, 0).c_str(), i < (maxopenfiles - 20) ? false : true, sizeof(T), readAheadSize);
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
					if (heap[p].m_data < heap[p / 2].m_data) {
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
		ExternalSortWriter<T>                   writer(m_path.c_str(), "wb", 1024 * 1024 * 1024 / WriteBufSize);
		KKLocalRef<ExternalSortWriter<T>::Job > writeJob = new ExternalSortWriter<T>::Job(WriteBufSize);
		DWORD t = ::timeGetTime(), t_display = t;
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

			if (written % 10000 == 0) {
				DWORD t2 = ::timeGetTime();
				if (t2 - t_display >= 2000) {
					printf("%llu/%llu items sorted...\n", written, m_totalItemCount);
					t_display = t2;
				}
			}

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
						files[fileInd].init(getPath(fileInd, splitIndexes[fileInd]).c_str(), files[fileInd].m_autoclose, sizeof(T), readAheadSize);
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
	long long nItems = N * 8 + 7;
	struct Null { };
	struct Item {
		long long val;
		Item(long long v = 0) : val(v) {}
		bool operator <(const Item& o) {
			return ::_byteswap_uint64(val) < ::_byteswap_uint64(o.val);
		}
		bool operator <=(const Item& o) {
			return ::_byteswap_uint64(val) <= ::_byteswap_uint64(o.val);
		}
		operator long long() const {
			return ::_byteswap_uint64(val);
		}
	};

	ExternalSorter<Item> sorter("ex_sort_test_data", std::vector<std::string>(), true, nThreads, N);
	DWORD t0 = ::timeGetTime();
	for (long long i = 0; i < nItems; i++) {
		sorter.push(((T)rand() << 32) | ((T)rand() << 16) | (T)rand());
	}
	DWORD t1 = ::timeGetTime();
	printf("\ninsert time:%lldms, external sort begins...\n", (long long)(t1 - t0));
	long long r = sorter.exSort(true, 0xffffffffull / sizeof(T));
	printf(" [%lld items] %s\n", r, (r == nItems ? "PASSED" : "FAILED!!!!!!"));
	return 0;
}

