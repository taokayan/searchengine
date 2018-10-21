
#pragma once 

#include "stdafx.h"
#include "KKBase.h"
#include "KKObject.h"

template <typename Key, 
	      typename Val = KKNul, 
          typename Base = KKNul,
		  typename Lock = KKLock,
		  typename NodeBase = KKNul>
class KKRBTree : public Base
{
private:
	KKRBTree(const KKRBTree &);
	KKRBTree & operator=(const KKRBTree &);
public:
  class Node;
  template <typename NodeT, bool isKKObject> struct NodeRet_;
  template <typename NodeT> struct NodeRet_<NodeT, true> {
    typedef KKLocalRef<NodeT> T;
  };
  template <typename NodeT> struct NodeRet_<NodeT, false> {
    typedef NodeT *T;
  };
  template <typename AnyBase> class Node_ : public AnyBase {
    KKAssert(!(KKSame<AnyBase, KKNul>::Val));
    friend class KKRBTree;
    friend class Node;
    Node *m_parent;
    Node *m_left;
    Node *m_right;
    Key  m_key;
    Val  m_val;
    unsigned char m_red;
    inline void addRef() {
      if (KKConverable<AnyBase, KKObject>::Val) ((KKObject*)this)->addRef();
    }
    inline void deRef() {
      if (KKConverable<AnyBase, KKObject>::Val) ((KKObject*)this)->deRef();
      else delete this;
    }
    inline Node_(const Key &t, const Val &v, Node *p, unsigned char red)
      :m_parent(p), m_left(0), m_right(0), m_key(t), m_val(v), m_red(red) { }
  };
  template <> class Node_<KKNul> {
    friend class Node;
    friend class KKRBTree;
    Node *m_parent;
    Node *m_left;
    Node *m_right;
    Key  m_key;
    Val  m_val;
    unsigned char m_red;
    inline void addRef() { } 
    inline void deRef() { delete this; }
    inline Node_(const Key &t, const Val &v, Node *p, unsigned char red)
      :m_parent(p), m_left(0), m_right(0), m_key(t), m_val(v), m_red(red) { }
  };
  class Node : public Node_<NodeBase> {
    friend class KKRBTree;
    inline Node(const Key &t, const Val &v, Node *p, unsigned char red)
                : Node_(t,v,p,red) { }
  public:
    inline const Key &key() const { return m_key; }
    inline const Val &val() const { return m_val; }
	inline Val &val() { return m_val; }
	inline void val(const Val &v) { m_val = v; }
  private:
    inline Node *sbling() {
      if (sizeof(void *) <= 4) {
	  return (Node *)(((unsigned int)m_parent->m_left) ^ 
                      ((unsigned int)m_parent->m_right) ^ 
                      ((unsigned int)this));
	  } else {
      return (Node *)(((unsigned long long)m_parent->m_left) ^ 
                      ((unsigned long long)m_parent->m_right) ^ 
                      ((unsigned long long)this));
	  }
    }
    inline bool hasChild() const {
      return (m_left || m_right);
    }
    inline bool hasBothChild() const {
      return (m_left && m_right);
    }
    inline int childCmp() const {
		if (m_left == 0) return 1;
		if (m_right == 0) return -1;
		return (int)m_right->m_red - (int)m_left->m_red;
    }
    inline Node *& child(int i) {
      return (&m_left)[i];
    }
    inline const Node *& child(int i) const {
      return (&m_left)[i];
    }
    void changeChild(Node *oldChild, Node *newChild) {
      if (m_left == oldChild) m_left = newChild;
      else m_right = newChild;
    }
    inline void setLeft(Node *l) {
      m_left = l; if (l) l->m_parent = this;
    }
    inline void setRight(Node *r) {
      m_right = r; if (r) r->m_parent = this;
    }
    int side() {
      return (int)(m_parent->m_left != this);
    }
  };
  typedef typename NodeRet_<Node, (KKConverable<Node *, KKObject *>::Val)>::T NodeRef;
private:
  Node *m_root;
  long long m_count;
  Lock m_lock;

private:
  inline void swapWithParent(Node *q)
  {
	  Node *p = q->m_parent;
	  Node *pp = p->m_parent;
      Node *l = p->m_left, *r = p->m_right;
      p->m_parent = q;
      q->m_parent = pp; 
	  if (pp) pp->changeChild(p, q); else m_root = q;
      p->setLeft(q->m_left);
      p->setRight(q->m_right);
      if (l == q) {
        q->m_left = p; q->setRight(r);
      } else {
        q->setLeft(l); q->m_right = p;
      }
	  unsigned char tred = p->m_red;
      p->m_red = q->m_red; q->m_red = tred;
  }

  // swap 2 nodes that don't have parent-child relationship
  inline void swapFarNode(Node *p, Node *q) 
  {
	  Node *pp = p->m_parent;
      Node *l = p->m_left, *r = p->m_right;
      p->m_parent = q->m_parent; 
	  if (p->m_parent) p->m_parent->changeChild(q, p); else m_root = p;
      q->m_parent = pp; 
	  if (q->m_parent) q->m_parent->changeChild(p, q); else m_root = q;
      p->setLeft(q->m_left);
      p->setRight(q->m_right);
      q->setLeft(l);
      q->setRight(r);
	  unsigned char tred = p->m_red;
	  p->m_red = q->m_red; q->m_red = tred;
  }
 // inline void swapNode(Node *p, Node *q) 
 // {
 //   if (p->m_parent == q) {
 //     swapWithParent(p);
 //   } else if (q->m_parent == p) {
//	  swapWithParent(q);
 //   } else {
//	  swapFarNode(p, q);
//	}
 // }
 // inline void rotateNode(Node *p, Node *q, Node *r) {
 //   swapNode(p, q);
 //   swapNode(q, r);
 // }
  template <int dir>
  void rotateTriangle(Node *p) {
	Node *pl, *pr, *pp = p->m_parent;
	if (dir == 0) {
		pl = p->m_left; pr = p->m_right; 
	} else {
		pl = p->m_right; pr = p->m_left; // mirror
	}
	unsigned char pred = p->m_red;
	p->m_parent = pr; p->m_left = pl->m_left; p->m_right = pl->m_right;
	pl->m_parent = pr; pl->m_left = pr->m_left; pl->m_right = pr->m_right;
	pr->m_parent = pp; 
	if (dir == 0) {
		pr->m_left = p; pr->m_right = pl;
	} else {
		pr->m_right = p; pr->m_left = pl;
	}
	if (pl->m_left) pl->m_left->m_parent = pl;
	if (pl->m_right) pl->m_right->m_parent = pl;
	if (pp) pp->changeChild(p, pr); else m_root = pr;
	p->m_red = pl->m_red; pl->m_red = pr->m_red; pr->m_red = pred;
	if (p->m_left) p->m_left->m_parent = p;
	if (p->m_right) p->m_right->m_parent = p;
  }

  template <int dir>
  void rotate(Node *r, int rotateType) { // dir == 0, counter-clock wise.
    Node *parent = r->m_parent;
    if (!r->child(1-dir) || (!r->child(1-dir)->hasChild())) return;
    if (rotateType == 1 || 
        (rotateType == 0 &&
         ((dir == 0 && r->child(1-dir)->childCmp() >= 0) ||
         (dir == 1 && r->child(1-dir)->childCmp() <= 0)))) {
   //   printf("rotation 1 at %d...", (int)r->m_key);
/****
         a                  b
       /   \              /   \
            b     =>     a     e
           / \          / \   
          d   e            d
*****/
      Node *a = r, *b = r->child(1-dir);
      { unsigned char col = b->m_red; b->m_red = a->m_red; a->m_red = col; }
      a->child(1-dir) = b->child(dir); 
      if (a->child(1-dir)) a->child(1-dir)->m_parent = a;
      a->m_parent = b; 
      b->child(dir) = a;
      b->m_parent = parent; if (parent) parent->changeChild(a, b); else m_root = b;
    } else {
   //   printf("rotation 2 at %d...", (int)r->m_key);
/****
        a                    d
      /   \_               /   \
             b     =>     a     b
            / \          / \   / \
           d                e f   
          / \
         e  f     
                (aedfb)
*****/
      Node *a = r, *b = r->child(1-dir), *d = b->child(dir);
      unsigned char a_red_old = a->m_red;
      a->m_parent = d; 
      a->m_red = b->m_red;
      a->child(1-dir) = d->child(dir); if (a->child(1-dir)) a->child(1-dir)->m_parent = a;
      b->m_parent = d; 
      b->child(dir) = d->child(1-dir); if (b->child(dir)) b->child(dir)->m_parent = b;
      b->m_red = d->m_red;
      d->m_parent = parent; if (parent) parent->changeChild(a, d); else m_root = d;
      d->child(dir) = a;
      d->child(1-dir) = b;
      d->m_red = a_red_old;
    }
  }
  inline void rotateLeft(Node *r, int rotateType = 0) { rotate<0>(r, rotateType); }
  inline void rotateRight(Node *r, int rotateType = 0) { rotate<1>(r, rotateType); }
  void doubleRed(Node *p) {
again:
	register Node *pp = p->m_parent;
    if (!pp) {
      p->m_red = false; return;
    }
    if (!pp->m_red) return; // not double-red
    // parent is also red
	Node *pps = pp->sbling();
    if (pps && pps->m_red) {
      pps->m_red = false;
      pp->m_red = false;
      pp->m_parent->m_red = true;
      p = pp->m_parent;
      goto again;
    } else { // rotate
      if (pp->side() == 1) {
        rotateLeft(pp->m_parent);
      } else {
        rotateRight(pp->m_parent);
      }
    }
  }
  void doubleBlack(Node *p) {
again:
    if (p->m_red) { p->m_red = false; return; }
    if (!p->m_parent) return;
    //printf("double black at %d...", (int)p->m_key);
    if (p->sbling()->m_red) {
      if (p->side() == 0) rotateLeft(p->m_parent, 1);
      else rotateRight(p->m_parent, 1);
    }
    Node *s = p->sbling();
    if (!s->m_left->m_red && !s->m_right->m_red) {
      s->m_red = true;
      p = p->m_parent;
      goto again;
    } else {
      if (p->side() == 0) {
        if (s->m_right->m_red) {
          rotateLeft(p->m_parent, 1);
          s->m_right->m_red = false; 
        } else {
          rotateLeft(p->m_parent, 2);
          s->m_red = false;
        }
      } else {
        if (s->m_left->m_red) {
          rotateRight(p->m_parent, 1);
          s->m_left->m_red = false;
        } else {
          rotateRight(p->m_parent, 2);
          s->m_red = false;
        }
      }
    }
  }
  int print_(int tab, Node *r, int tabstep, 
             Key &outMin, Key &outMax, bool validateOnly = false) {
    if (!r) return 0;
    int bcl = 0, bcr = 0;
    Key leftMax = r->m_key, rightMin = r->m_key;
    if (r->m_right) bcr = print_(tab + 1, r->m_right, tabstep,
                                 rightMin, outMax, validateOnly);
    if (!validateOnly) {
      for(int i = 0 ; i < tab * tabstep; i++) printf(" ");
      std::cout << r->m_key;
    }
    bool valid = true;
    if (r->m_red) {
      if (r->m_parent && r->m_parent->m_red) valid = false;
      if (!validateOnly)
        printf("(r)%s\n", (valid ? "":"FAILED double red"));
    } else if (!validateOnly) printf("\n");
    if (r->m_left) bcl = print_(tab + 1, r->m_left, tabstep,
                                outMin, leftMax, validateOnly);
	if (r->m_key < leftMax) valid = false;
    if (r->m_key > rightMin) valid = false;
    if (!valid) return -1;
    if (bcr != bcl || bcr < 0) return -1;
    else return 1 - (int)r->m_red + bcl;
  }
public:
  KKRBTree() : m_root(0), m_count(0) { }
  bool validate() {
    KKLockGuard<Lock> g(m_lock);
    if (!m_root) return true;
    Key min, max;
    return print_(0, m_root, 0, min, max, true) >= 0;
  }
  int print(int tabstep = 4) { // return negative for invalid tree
    KKLockGuard<Lock> g(m_lock);
    if (!m_root) { printf("\n(empty tree)\n"); return 0; }
    printf("\n");
    Key min, max;
    int r = print_(0, m_root, tabstep, min, max);
    if (r < 0) printf("FAILED! Depth");
    else printf("Black Depth %d", r);
    return r;
  }
  NodeRef insert(const Key &t, const Val &v) {
    KKLockGuard<Lock> g(m_lock);
	m_count++;
    if (!m_root) {
      m_root = new Node(t, v, 0, false);
      m_root->addRef();
      return m_root;
    } else {
      Node *p = m_root;
      while (true) {
        if (t < p->m_key) {
          if (!p->m_left) {
            p->m_left = new Node(t, v, p, true);
            p = p->m_left;
            break;
          }
          p = p->m_left;
        } else {
          if (!p->m_right) {
            p->m_right = new Node(t, v, p, true);
            p = p->m_right;
            break;
          }
          p = p->m_right;
        }
      }
      p->addRef();
      doubleRed(p);
      return p;
    }
  }  
private:
  inline Node *find_(const Key &t) {
    Node *p = m_root;
    while (true) {
      if (!p) return 0;
      if (t > p->m_key) p = p->m_right;
      else if (t < p->m_key) p = p->m_left;
      else return p;
    }
  }
public:
  NodeRef find(const Key &t) {
    KKLockGuard<Lock> g(m_lock);
    return NodeRef(find_(t));
  }
  NodeRef findInsert(const Key &key, const Val &val, bool override_ = false)
  {
	KKLockGuard<Lock> g(m_lock);
    Node *p = find_(key);
	if (p) {
		if (override_) p->m_val = val;
		return NodeRef(p);
	} else {
		return insert(key, val);
	}
  }
  bool remove(Node *p) {
	if (!p) return false;
    KKLockGuard<Lock> g(m_lock);
	if (!p->m_parent && p != m_root) return false;
	m_count--;
    if (p->hasBothChild()) {
      Node *q = p->m_right;
	  if (!q->m_left) {
		swapWithParent(q);
	  } else {
		q = q->m_left;
		while (q->m_left) q = q->m_left;
		swapFarNode(p, q);
	  }
    } else if (!p->m_parent) {
      m_root = p->m_left ? p->m_left: p->m_right;
      if (m_root) {
        m_root->m_parent = 0;
        m_root->m_red = false;
      }
      goto finished;
    }
again:
    if (p->hasChild()) { // one red child only
      Node *c = (Node *)((unsigned long long)p->m_left + 
                         (unsigned long long)p->m_right);
      p->m_parent->changeChild(p, c);
      c->m_parent = p->m_parent;
      c->m_red = false;
    } else {
      if (p->m_red) {
        p->m_parent->changeChild(p, 0);
        goto finished;
      }
      Node *s = p->sbling();
      if (s->hasChild()) {
        Node *replace = s;
        if (p->side() == 0) {
		  if (!replace->m_left) {
			  rotateTriangle<0>(p->m_parent);
			 // swapWithParent(p); swapWithParent(replace);
		  } else {
			  replace = replace->m_left;
			  while (replace->m_left) replace = replace->m_left;
			  swapWithParent(p); swapFarNode(p, replace);
		  }
        } else {
		  if (!replace->m_right) {
			  //swapWithParent(p); swapWithParent(replace);
			  rotateTriangle<1>(p->m_parent);
		  } else {
			  replace = replace->m_right;
			  while (replace->m_right) replace = replace->m_right;
			  swapWithParent(p); swapFarNode(p, replace);
		  }
        }
        goto again;
      } else {
        p->m_parent->changeChild(p, 0);
        s->m_red = true;
        doubleBlack(p->m_parent);
      }
    }
finished:
	if (KKConverable<Node *, KKObject *>::Val) {
		p->m_parent = p->m_left = p->m_right = 0;
	}
	p->deRef();
    return true;
  }
  bool remove(const Key &t) {
    KKLockGuard<Lock> g(m_lock);
    Node *p = find(t);
    if (!p) return false;
    remove(p);
    return true;
  }
  long long count() const {
	  return m_count;
  }
	NodeRef first() {
		if (m_count == 0) return 0;
		KKLockGuard<Lock> g(m_lock);
		Node *p = m_root;
		if (!p) return 0;
		while (p->m_left) p = p->m_left;
		return NodeRef(p);
	}
	NodeRef last() {
		if (m_count == 0) return 0;
		KKLockGuard<Lock> g(m_lock);
		Node *p = m_root;
		if (!p) return 0;
		while (p->m_right) p = p->m_right;
		return NodeRef(p);
	}
	NodeRef next(Node *c) {
		KKLockGuard<Lock> g(m_lock);
		if (!c) return 0;
		if (c->m_right == 0) {
			while (c->m_parent && c->m_parent->m_right == c) c = c->m_parent;
			return c->m_parent;
		}
		c = c->m_right;
		while (c->m_left) c = c->m_left;
		return NodeRef(c);
	}
	NodeRef prev(Node *c) {
		KKLockGuard<Lock> g(m_lock);
		if (!c) return 0;
		if (c->left == 0) {
			while (c->m_parent && c->m_parent->m_left == c) c = c->m_parent;
			return c->m_parent;
		}
		c = c->left;
		while (c->m_right) c = c->m_right;
		return NodeRef(c);
	}
};
