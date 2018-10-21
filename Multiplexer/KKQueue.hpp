
#pragma once

#include "KKBase.h"
#include "KKLock.h"
#include "KKLockGuard.h"
#include "KKObject.h"
#include "KKAtomic.hpp"
#include "KKHash.hpp"

// A queue that has no push lock
template <typename T, typename Base = KKNul, typename Lock = KKLock, typename NodeBase = KKObject>
class KKQueue : public Base {
public:
	class Node;
	class Node : public NodeBase {
		friend class KKQueue;
#pragma pack(push, 8)
		Node * volatile       m_next;
		T                     m_val;
#pragma pack(pop)
		Node(const Node&);
	public:
		inline Node(const T &t) : m_next(0), m_val(t) { } 
		inline T &get() { return m_val; }
	};
private:
	typedef KKLockGuard<Lock> Guard;
	KKAtomic<long long> m_count;
	KKAtomic<Node *>    m_head;
	KKAtomic<Node *>    m_tail;
	Lock                m_popLock;
	bool                m_pending;

private:
	KKQueue(const KKQueue &);
	KKQueue &operator=(const KKQueue &);

public:
	KKQueue() : m_count(0), m_head(0), m_tail(0), m_pending(false) { }
	inline KKLocalRef<Node> push(Node *node) {
		KKLocalRef<Node> ret = node;
		node->addRef();
		volatile Node * volatile oldtail = m_tail.xch(node);
		if (oldtail) oldtail->m_next = node;
		else m_head = node;
		m_count++;
		return ret;
	}
	inline KKLocalRef<Node> push(const T &t) {
		Node *node = new Node(t);
		return push(node);
	}
	inline KKLocalRef<Node> pop() {
		if (!m_count) return KKLocalRef<Node>();
		Guard g(m_popLock);
		if (m_pending) {
			Node *node = (Node *)(m_head->m_next);
			if (!node) return KKLocalRef<Node>();
			m_count--;
			m_head->deRef();
			m_head = node;
			return node;
		} else {
			m_count--;
			m_pending = true;
			return KKLocalRef<Node>(m_head);
		}
	}
	inline long long count() const { return m_count; }
};

// semi Lock-free queue
template <typename T, typename Base = KKNul, typename NodeBase = KKObject>
class KKQueue2 : public Base {
public:
	class Node;
	class Node : public NodeBase {
		friend class KKQueue2;
#pragma pack(push, 8)
		Node * volatile  m_next;
		KKAtomic<int>    m_taken;
		T                m_val;
#pragma pack(pop)
		Node(const Node&);
	public:
		inline Node(const T &t) : m_next(0), m_taken(0), m_val(t) { } 
		inline T &get() { return m_val; }
	};
private:
	KKAtomic<long long> m_count;
	KKAtomic<Node*>     m_head;
	KKAtomic<Node*>     m_tail;
private:
	KKQueue2(const KKQueue2 &);
	KKQueue2 &operator=(const KKQueue2 &);

public:
	KKQueue2() : m_count(0), m_head(0), m_tail(0) { }
	inline KKLocalRef<Node> push(Node *node) {
		KKLocalRef<Node> ret = node;
		node->addRef();
		Node* oldtail = m_tail.xch(node);
		if (!oldtail) {
			m_head = node;
		} else {
			oldtail->m_next = node;
		}
		m_count++;
		return ret;
	}
	inline KKLocalRef<Node> push(const T &t) {
		Node *node = new Node(t);
		return push(node);
	}
	inline KKLocalRef<Node> pop_() { // no free
retry:
		if (!m_count) return KKLocalRef<Node>();
		volatile Node *h = m_head;
		if (h->m_next) {
			if (m_head.cmpXch(h->m_next, (Node *)h) != h) goto retry;
		}
		if (h->m_taken || h->m_taken.cmpXch(1,0)) goto retry;
		m_count--;
		return KKLocalRef<Node>((Node *)h);
	}

	inline KKLocalRef<Node> pop() { // with free
retry:
		if (!m_count) return KKLocalRef<Node>();
		volatile Node *h = m_head.xch((Node *)1);
		if (!h) return KKLocalRef<Node>();
		if (h == (Node *)1) {
			KKLock_Yield();
			goto retry;
		}
		if ((int)(h->m_taken) && h->m_next) {
			m_head = h->m_next;
			((Node *)h)->deRef();
			goto retry;
		}
		if (!(h->m_taken)) {
			KKLocalRef<Node> ret = (Node *)h;
			h->m_taken = 1;
			m_head = (Node *)h;
			m_count--;
			return ret;
		}
		m_head = (Node *)h;
		return KKLocalRef<Node>();
	}
	inline long long count() const { return m_count; }
};
