#pragma once

#include "KKBase.h"
#include "KKLock.h"
#include "KKObject.h"

template <typename Key, typename Val,
          typename Base = KKNul, typename NodeBase = KKNul,
          typename Lock = KKNoLock>
class KKFibonacciHeap : public Base
{
private:
	KKFibonacciHeap(const KKFibonacciHeap&);
	KKFibonacciHeap &operator=(const KKFibonacciHeap&);
  // max number of nodes ~= 2 ^ (MaxRank / 1.5)
  enum _ { MaxRank = 48 }; 
  template <typename NodeT> 
  inline void addRef_(NodeT *node) {
    if (KKConverable<NodeT *, KKObject *>::Val) {
      ((KKObject *)node)->addRef();
    }
  }
  template <typename NodeT> 
  inline void deRef_(NodeT *node) {
    if (KKConverable<NodeT *, KKObject *>::Val) {
      ((KKObject *)node)->deRef();
    } else {
      delete node;
    }
  }
  template <typename NodeT, bool isKKObject> struct NodeRet_;
  template <typename NodeT> struct NodeRet_<NodeT, true> {
    typedef KKLocalRef<NodeT> T;
  };
  template <typename NodeT> struct NodeRet_<NodeT, false> {
    typedef NodeT *T;
  };

public:
	class Node : public NodeBase
  {
    friend class KKFibonacciHeap;
    unsigned char m_nChilds;
    unsigned char m_marked;
		Node *m_prev, *m_next, *m_parent, *m_child;
		Key m_key;
    Val m_val;
		Node(const Key &k, const Val &val) 
      : m_prev(this), m_next(this), m_parent(0),
      m_child(0), m_key(k), m_val(val), 
      m_nChilds(0), m_marked(false) { } 
		Node(Node *p, Node *n, const Key &k, const Val &val) 
      : m_prev(p), m_next(n), m_parent(0),
      m_child(0), m_key(k), m_val(val), 
      m_nChilds(0), m_marked(false) { } 
  public:
    const Key& key() const { return m_key; }
    const Val &val() const { return m_val; }
	};
  typedef typename NodeRet_<Node, (KKConverable<Node *, KKObject *>::Val)>::T NodeRef;

private:
	Node *m_root;
	Lock m_lock;

  void print_(Node *p, int tab, int dtab) {
    for(int i = 0; i < tab; i++) printf(" ");
    std::cout << p->m_key;
    if (p->m_marked) printf("(*)");
    printf("\n");
    if (p->m_child) {
      Node *c = p->m_child;
      do {
        print_(c, tab + dtab, dtab);
        c = c->m_next;
      } while (c != p->m_child);
    }
  }
  template <bool safe>
  inline Node *takeout_(Node *c) {
    if (safe) {
      if (!c->m_parent) return 0;
    }
    c->m_parent->m_nChilds--;
    if (c->m_next == c) {
      c->m_parent->m_child = 0;
    } else {
      if (c->m_parent->m_child == c) c->m_parent->m_child = c->m_next;
      c->m_next->m_prev = c->m_prev;
      c->m_prev->m_next = c->m_next;
    }
    Node *p = c->m_parent;
    if (safe) { c->m_next = c->m_prev = c->m_parent = 0;}
    return p;
  }
  template <bool updateRoot>
  inline void insertToRoot_(Node *p) {
    p->m_parent = 0;
    p->m_marked = false;
    Node *rn = m_root->m_next;
    m_root->m_next = p;
    p->m_next = rn;
    rn->m_prev = p;
    p->m_prev = m_root;
    if (updateRoot) { if (p->m_key < m_root->m_key) m_root = p; }
  }
  void insertTo_(Node *p, Node *c) {
    p->m_nChilds++;
    if (!p->m_child) {
      p->m_child = c;
      c->m_next = c->m_prev = c;
    } else {
      Node *pcn = p->m_child->m_next;
      p->m_child->m_next = c;
      c->m_next = pcn;
      pcn->m_prev = c;
      c->m_prev = p->m_child;
    }
    c->m_parent = p;
  }
  void release_(Node *p) {
    if (!p) return;
    Node *r = p;
    do {
      Node *rn = r->m_next;
      release_(r->m_child);
      deRef_(r);
      r = rn;
    } while (r != p);
  }
public:
	KKFibonacciHeap() : m_root(0) { }
	~KKFibonacciHeap() { release_(m_root); }
  void print() {
    KKLockGuard<Lock> g(m_lock);
    if (!m_root) printf("(empty)\n");
    else {
      Node *p = m_root;
      do {
        print_(p, 0, 4);
        p = p->m_next;
      } while (p != m_root);
    }
  }

	NodeRef insert(const Key &k, const Val &v) {
		KKLockGuard<Lock> g(m_lock);
		if (!m_root) {
			m_root = new Node(k, v);
      addRef_(m_root);
      return m_root;
		} else {
		  Node *node = new Node(m_root, m_root->m_next, k, v);
      addRef_(node);
		  node->m_next->m_prev = node;
		  node->m_prev->m_next = node;
		  if (k < m_root->m_key) m_root = node;
      return node;
    }
	}
  inline void decreaseKey(Node *node_, const Key &k) {
    KKLockGuard<Lock> g(m_lock);
    Node *node = node_;
    node->m_key = k;
    if (!node->m_parent) {
      if (node->m_key < m_root->m_key) m_root = node;
      return;
    }
    if (node->m_key >= node->m_parent->m_key) return;
    do {
      Node *parent = takeout_<false>(node);
      insertToRoot_<false>(node);
      if (!parent->m_marked) {
        if (parent->m_parent) parent->m_marked = true;
        break;
      }
      node = parent;
    } while (node->m_parent);
    if (node_->m_key < m_root->m_key) m_root = node_;
  }
  void deleteMin() {
    KKLockGuard<Lock> g(m_lock);
    if (!m_root) return;
    Node *p = m_root;
    if (m_root->m_child) { // take out all the childs
      Node *c = m_root->m_child;
      do {
        Node *cn = c->m_next;
        insertToRoot_<false>(c);
        c = cn;
      } while (c != m_root->m_child);
    }
    else if (m_root->m_next == m_root) {
      m_root = 0; deRef_(p); return;
    }
    p->m_next->m_prev = p->m_prev;
    p->m_prev->m_next = p->m_next;
    m_root = m_root->m_next;
    Node *tree[MaxRank];
    int curMax = 0;
    tree[0] = 0;
    Node *r = m_root;
    do {
      Node *rn = r->m_next;
      int rank = r->m_nChilds;
again:
      if (rank > curMax || !tree[rank]) {
        if (rank > curMax) {
         // memset(&(tree[curMax+1]), 0, sizeof(tree[0]) * (rank - curMax));
          for (int i = curMax + 1; i < rank; i++) tree[i] = 0;
          curMax = rank;
        }
        tree[rank] = r;
      } else {
        Node *q = tree[rank];
        tree[rank] = 0;
        if (q->m_key < r->m_key) {
          Node *s = q; q = r; r = s;
        }
        if (m_root == q) m_root = m_root->m_next;
        q->m_next->m_prev = q->m_prev;
        q->m_prev->m_next = q->m_next;
        insertTo_(r, q);
        rank++;
        goto again;
      }
      r = rn;
    } while (r != m_root && !r->m_parent);
    r = m_root->m_next;
    Node *minNode = m_root;
    while (r != m_root) {
      if (r->m_key < minNode->m_key) minNode = r;
      r = r->m_next;
    }
    m_root = minNode;
    deRef_(p);
  }
	NodeRef getMin() {
		KKLockGuard<Lock> g(m_lock);
    return m_root;
	}
};


