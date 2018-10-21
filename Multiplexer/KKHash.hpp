
#pragma once

#include "KKNodeBase.hpp"
#include <string>

template <typename Key>
struct KKHashFn;

typedef unsigned long long KKHashVal;

#define KKGolden64 (0x9e3779b97f4a7c13ull)

template <> struct KKHashFn<signed char> {
	inline static KKHashVal hash(signed char v) { 
		return KKGolden64 * (KKHashVal)(long long)v; }
};
template <> struct KKHashFn<unsigned char> {
	inline static KKHashVal hash(unsigned char v) { 
		return KKGolden64 * (KKHashVal)v; }
};
template <> struct KKHashFn<signed short> {
	inline static KKHashVal hash(signed short v) { 
		return KKGolden64 * (KKHashVal)(long long)v; }
};
template <> struct KKHashFn<unsigned short> {
	inline static KKHashVal hash(unsigned short v) { 
		return KKGolden64 * (KKHashVal)v; }
};
template <> struct KKHashFn<int> {
	inline static KKHashVal hash(int v) { 
		return KKGolden64 * (KKHashVal)(long long)v; }
};
template <> struct KKHashFn<unsigned int> {
	inline static KKHashVal hash(unsigned int v) { 
		return KKGolden64 * (KKHashVal)v; }
};
template <> struct KKHashFn<long long> {
	inline static KKHashVal hash(long long v) { 
		return KKGolden64 * (KKHashVal)v; }
};
template <> struct KKHashFn<unsigned long long> {
	inline static KKHashVal hash(unsigned long long v) { 
		return KKGolden64 * (KKHashVal)v; }
};
template <> struct KKHashFn<float> {
	inline static KKHashVal hash(float v) { 
		double t = v; return KKGolden64 * *(KKHashVal*)&t; }
};
template <> struct KKHashFn<double> {
	inline static KKHashVal hash(double v) { 
		return KKGolden64 * *(KKHashVal*)&v; }
};
template <> struct KKHashFn<const char*> {
	inline static KKHashVal hash(const char *s) { 
		KKHashVal v = 0;
		if (!s) return 0;
		while (*s) {
			v = (v * KKGolden64) ^ (*s);
			v ^= (v >> 32);
			s++;
		}
		return v;
	}
};
template <> struct KKHashFn<std::string> {
	inline static KKHashVal hash(const std::string &key) { 
		return KKHashFn<const char*>::hash(key.c_str());
	}
};

template <typename Key, 
	      typename Val = KKNul, 
	      typename Base = KKNul, 
		  typename Lock = KKLock, 
          typename NodeBase = KKNul,
		  typename HashFn = KKHashFn<Key>,
          bool UseNodeBuffer = false>
class KKHash : public Base
{
private:
	KKHash(const KKHash &); // prevent misuse
	KKHash operator=(const KKHash&);
public:
	template <typename T> struct Node_ : public T {
		Node_  *m_next;
		Key    m_key;
		Val    m_val;
	};
	template <> struct Node_<KKNul> {
		Node_  *m_next;
		Key    m_key;
		Val    m_val;
	};
	typedef Node_<NodeBase> Node;
	typedef typename KKNodeRetType<Node>::T NodeRef;

private:
	inline int nodeCmp_(Node *p1, Node *p2) {
		KKHashVal h1 = HashFn::hash(p1->m_key);
		KKHashVal h2 = HashFn::hash(p2->m_key);
		if (h1 < h2) return -1;
		if (h1 > h2) return 1;
		if (p1 < p2) return -1;
		if (p1 > p2) return 1;
		return 0;
	}
	inline size_t index_(KKHashVal hash, unsigned char bit) {
		// use highest bits first to maintain hashVal in order
		return (size_t)(hash >> (sizeof(KKHashVal) * 8 - bit)); 
	}
	inline void updateRange_() {
		m_upCount = m_nodesPerBucket * (1ull<<m_nBits);
		if (m_nBits <= m_minBits) m_downCount = 0;
		else m_downCount = m_nodesPerBucket * (1ull<<m_nBits) * 3 / 8;
	}
	template <bool inc>
	inline void resize_() {
		unsigned char newBits = m_nBits;
		if (inc) {
		  if (m_count > m_upCount) newBits++;
		  else return;
		} else {
		  if (m_count < m_downCount) newBits--;
		  else return;
		}
		size_t newSize = (1ull << newBits);
		Node **bucket = new Node*[newSize];
		for (size_t i = 0; i < newSize; i++) bucket[i] = 0;
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			Node *p = m_bucket[i];
			while (p) {
				Node *pnext = p->m_next;
				KKHashVal h = HashFn::hash(p->m_key);
				size_t newInd = index_(h, newBits);
				p->m_next = bucket[newInd];
				bucket[newInd] = p;
				p = pnext;
			}
		}
		delete[] m_bucket;
		m_bucket = bucket;
		m_nBits = newBits;
		updateRange_();
	}
	inline void destroy(Node *p) {
		if (UseNodeBuffer &&
			(p >= m_nodeBuf && p < m_nodeBuf + m_nodeBufCap)) {}
		else KKNode_deRef(p);
	}
public:
	void clear() {
		KKLockGuard<Lock> g(m_lock);
		m_nodeBufUsed = 0;
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			while (m_bucket[i]) {
				Node *p = m_bucket[i];
				m_bucket[i] = m_bucket[i]->m_next;
				destroy(p);
			}
		}
		delete[] m_bucket;

		m_count = 0;
		m_nBits = m_minBits;
		m_bucket = new Node*[(1ull<<m_nBits)];
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			m_bucket[i] = 0; 
		}
		updateRange_();
	}
	KKHash(int bucketBits = 4, int nodesPerBucket = 1,
		   Node *nodeBuffer = 0, uint32_t nodebufSize = 0) {
		m_count = 0;
		m_nodesPerBucket = nodesPerBucket;
		if (bucketBits < 1) bucketBits = 1;
		m_minBits = m_nBits = bucketBits;
		m_bucket = new Node*[(1ull<<m_nBits)];
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			m_bucket[i] = 0; 
		}
		updateRange_();
		if (UseNodeBuffer) {
			m_nodeBufUsed = 0;
			m_nodeBufCap = nodebufSize;
			m_nodeBuf = nodeBuffer;
		}
	}
	~KKHash() {
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			while (m_bucket[i]) {
				Node *p = m_bucket[i];
				m_bucket[i] = m_bucket[i]->m_next;
				destroy(p);
			}
		}
		delete[] m_bucket;
	}
	size_t count() const { return m_count; }
	NodeRef randNode() {
		size_t r = rand() % (1ull<<m_nBits);
		KKLockGuard<Lock> g(m_lock);
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			size_t j = (i + r);
			if (j >= (1ull<<m_nBits)) j -= (1ull<<m_nBits);
			if (m_bucket[j]) return m_bucket[j];
		}
		return (Node *)0;
	}
	NodeRef first() {
		KKLockGuard<Lock> g(m_lock);
		Node *min = 0;
		for (size_t i = 0; i < (1ull<<m_nBits); i++) {
			if (m_bucket[i]) {
				min = m_bucket[i];
				for (Node *p = min->m_next; p; p = p->m_next) {
					if (nodeCmp_(min, p) > 0) min = p;
				}
				break;
			}
		}
		return min;
	}
	NodeRef next(Node *node) {
		if (!node) return (Node *)0;
		KKHashVal h = HashFn::hash(node->m_key); 
		KKLockGuard<Lock> g(m_lock);
		size_t ind = index_(h, m_nBits);
		Node *min = 0;
		for (size_t i = ind; i < (1ull<<m_nBits); i++) {
			for (Node *p = m_bucket[i]; p; p = p->m_next) {
				if (nodeCmp_(p, node) > 0 &&
					(!min || nodeCmp_(min, p) > 0)) min = p;
			}
			if (min) return min;
		}
		return (Node *)0;
	}
	inline NodeRef end() { return (Node *)0; }
	inline NodeRef insert(const Key &k, const Val &v = Val()) {
		KKHashVal h = HashFn::hash(k); 

		KKLockGuard<Lock> g(m_lock);
		Node *n;
		if (UseNodeBuffer && m_nodeBufUsed < m_nodeBufCap) {
			n = &(m_nodeBuf[m_nodeBufUsed++]);
		}
		else n = new Node;
		KKNode_addRef(n);
		n->m_key = k, n->m_val = v;
		size_t ind = index_(h, m_nBits);
		n->m_next = m_bucket[ind];
		m_bucket[ind] = n;
		m_count++;
		resize_<true>();
		return NodeRef(n);
	}
	inline NodeRef find(const Key &k) {
		if (!m_count) return (Node *)0;
		KKHashVal h = HashFn::hash(k);
		KKLockGuard<Lock> g(m_lock);
		size_t ind = index_(h, m_nBits);
		for (Node *p = m_bucket[ind]; p; p=p->m_next) {
			if (p->m_key == k) return p;
		}
		return (Node *)0;
	}
	inline NodeRef findInsert(const Key &k, const Val &v, bool override_ = true) {
		KKLockGuard<Lock> g(m_lock);
		NodeRef n = find(k);
		if (n) {
			if (override_) n->m_val = v;
			return n;
		}
		return insert(k, v);
	}
	inline bool remove(const Key &k) {
		KKHashVal h = HashFn::hash(k);
		KKLockGuard<Lock> g(m_lock);
		size_t ind = index_(h, m_nBits);
		Node *pre = 0;
		Node *p;
		for (p = m_bucket[ind]; p; p=p->m_next) {
			if (p->m_key == k) {
				if (!pre) {
					m_bucket[ind] = p->m_next;
					goto found;
				} else {
					pre->m_next = p->m_next;
					goto found;
				}
			}
			pre = p;
		}
		return false;
found:
		destroy(p);
		m_count--;
		resize_<false>();
		return true;
	}
	inline bool remove(const Node *node) {
		KKHashVal h = HashFn::hash(node->m_key);
		KKLockGuard<Lock> g(m_lock);
		size_t ind = index_(h, m_nBits);
		Node *pre = 0, *p;
		for (p = m_bucket[ind]; p; p=p->m_next) {
			if (p == node) {
				if (!pre) {
					m_bucket[ind] = p->m_next;
					goto found;
				} else {
					pre->m_next = p->m_next;
					goto found;
				}
			}
			pre = p;
		}
		return false;
found:
		destroy(p);
		m_count--;
		resize_<false>();
		return true;
	}
private:
	Node           **m_bucket;
	Node           *m_nodeBuf;
	uint32_t       m_nodeBufUsed, m_nodeBufCap;
	size_t         m_count;
	size_t         m_upCount;
	size_t         m_downCount;
	size_t         m_nodesPerBucket;
	Lock           m_lock;
	unsigned char  m_minBits;
	unsigned char  m_nBits;
};

KKAssert(sizeof(KKHash<int,int>::Node) == sizeof(void*) + 8);
