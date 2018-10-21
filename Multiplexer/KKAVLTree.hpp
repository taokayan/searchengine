
#pragma once
#include "KKBase.h"
#include "KKNodeBase.hpp"
#include "KKLock.h"
#include "KKLockGuard.h"
#include "KKHeap.hpp"
#include <sstream>
#include <iostream>

template <typename Key, 
	      typename Val = KKNul, 
	      typename Base = KKNul, 
		  typename Lock = KKLock, 
          typename NodeBase = KKNul>
class KKAVLTree : public Base
{
	typedef typename KKLockGuard<Lock> Guard;
private:
	// prevent mis-use
	KKAVLTree(const KKAVLTree&);
	KKAVLTree &operator=(const KKAVLTree &);

public:
	class Node;
private:
	class NodeData_ {
	public:
		Node *m_parent;
		Node *m_left;
		Node *m_right;
		Key  m_key;
		Val  m_val;
		short m_height;
	};
	template <typename B> class Node_ : public B, public NodeData_ { };
	template <> class Node_<KKNul> : public NodeData_ { };
public:
	class Node : public Node_<NodeBase> {
		friend class KKAVLTree;
		inline Node * &child(int i) {
			return (&m_left)[i];
			//return i == 0 ? m_left : m_right;
		}
	private:
		inline void calHeightNoCheck_() {
			m_height = m_left->m_height > m_right->m_height ? 
				       m_left->m_height + 1 : m_right->m_height + 1;
		}
		inline void calHeight() {
			short lh = m_left ? m_left->m_height : 0;
			short rh = m_right ? m_right->m_height : 0;
			m_height = lh > rh ? lh + 1 : rh + 1;
		}
	public:
		inline Node(const Key &k, const Val &v, Node *parent = 0) {
			m_parent = parent, m_left = m_right = 0,
			m_key = k, m_val = v, m_height = 1; 
		}
		inline const Key &key() const { return m_key; }
		inline Val &val() { return m_val; }
		inline Val &val() const { return m_val; }
		inline void val(const Val &v) { m_val = v; }
	};
	typedef typename KKNodeRetType<Node>::T NodeRef;
	enum _ { NodeSize = sizeof(Node) };
private:
	
	static KKHeap<NodeSize > s_heap;

#pragma pack(push, 8)
	Node *m_root;
	long long m_count;
#pragma pack(pop)
	Lock m_lock;
	
private:
	void release(Node *p) {
		if (!p) return;
		release(p->m_left);
		release(p->m_right);
		KKNode_deRef(p);
	}

public:
	inline KKAVLTree() : m_root(0), m_count(0) { }
	inline ~KKAVLTree() {
		release(m_root);
		m_root = 0;
	}
	Lock &lock() { return m_lock; }
	inline long long count() const { return m_count; }
private:

	template <int dir>
	inline Node *rotate(Node *p)  // dir == 0 for clockwise, 1 for counter-clockwise
	{
		const int opp = 1 - dir; // oppsite child
		Node *l = p->child(dir);
		if (!l) return 0;
		Node *r = p->child(opp), *root = p->m_parent;
		short lh = l->m_height, rh = r ? r->m_height : 0;
		if (lh > rh + 1) {
			//if (lh > rh + 2) {
			//	if (dir == 0)
			//		printf("Fatal KKAVLTree, lh > rh + 2\n");
			//	else printf("Fatal KKAVLTree, rh > lh + 2\n");
			//}
			// left is too heavy
			Node *a = l->child(dir), *b = l->child(opp);
			if (!b) goto rotate_type1;
			if (!a) goto rotate_type2;
			if (a->m_height >= b->m_height) {
rotate_type1:
/**                root           root
				   |              |
                   P              L
                  / \            / \
				 L   R   to     A   P
                / \                / \
			   A   B              B   R
**/
				p->child(dir) = b; if (b) b->m_parent = p;
				l->child(opp) = p, p->m_parent = l;
				l->m_parent = root;
				if (root) {
					if (root->m_left == p) root->m_left = l;
					else root->m_right = l;
				} else {
					m_root = l;
				}
				short oldh = p->m_height;
				p->calHeight();
				l->calHeightNoCheck_();
				if (oldh != l->m_height) return root;
				return 0;
			} else {
rotate_type2:
/**                P              B
                  / \            / \
				 L   R   to     L    P
                / \            / \  / \
			   A   B          A   m n  R
                  / \
				 m   n
**/
				Node *m = b->child(dir), *n = b->child(opp);
				p->child(dir) = n; if (n) n->m_parent = p;
				b->child(opp) = p, p->m_parent = b;
				b->child(dir) = l, l->m_parent = b;
				l->child(opp) = m; if (m) m->m_parent = l;
				b->m_parent = root;
				if (root) {
					if (root->m_left == p) root->m_left = b;
					else root->m_right = b;
				} else {
					m_root = b;
				}
				short oldh = p->m_height;
				p->calHeight();
				l->calHeight();
				b->calHeightNoCheck_();
				if (oldh != b->m_height) return root;
				return 0;
			}
		}
		return 0;
	}

	template <bool hasChild>
	inline void adjust(Node *p) 
	{
		if (!hasChild && !p) return;
		while (true) {
			short oldh = p->m_height;
			p->calHeight();
			Node *parent = rotate<0>(p);
			if (parent) {
				p = parent;
				if (!hasChild) { adjust<true>(p); return; }
				else 
					continue;
			}
			parent = rotate<1>(p);
			if (parent) {
				p = parent;
				if (!hasChild) { adjust<true>(p); return; }
				else 
				continue;
			}
			if (oldh < p->m_height) {
				parent = p->m_parent;
				if (!parent) return;
				p = parent;
				if (!hasChild) adjust<true>(p);
				else 
				continue;
			} else if (oldh > p->m_height) {
				parent = p->m_parent;
				if (!parent) return;
				p = parent;
				if (!hasChild) adjust<true>(p);
				else 
				continue;
			}
			return;
		}
	}

	inline void xchWithParent(Node *p) {
		//if (!p) return;
		Node *pp = p->m_parent, *pl = p->m_left, *pr = p->m_right;
		//if (!pp) return;
		short pph = pp->m_height;
		pp->m_height = p->m_height;
		p->m_height = pph;
		if (pp->m_left == p) {
			p->m_left = pp;
			p->m_right = pp->m_right;
			if (p->m_right) p->m_right->m_parent = p;
		} else {
			p->m_left = pp->m_left;
			if (p->m_left) p->m_left->m_parent = p;
			p->m_right = pp;
		}
		p->m_parent = pp->m_parent; 
		if (pp->m_parent) {
			if (pp->m_parent->m_left == pp) pp->m_parent->m_left = p;
			else pp->m_parent->m_right = p;
		}
		pp->m_parent = p;
		pp->m_left = pl; if (pl) pl->m_parent = pp;
		pp->m_right = pr; if (pr) pr->m_parent = pp;
	}
	inline void xchFarNode(Node *p, Node *q) {
		Node *pp = p->m_parent, *pl = p->m_left, *pr = p->m_right;
		Node *qp = q->m_parent, *ql = q->m_left, *qr = q->m_right;
		short ph = p->m_height, qh = q->m_height;
		if (p->m_parent) {
			if (p->m_parent->m_left == p) p->m_parent->m_left = q;
			else p->m_parent->m_right = q;
		}
		if (p->m_left) p->m_left->m_parent = q;
		if (p->m_right) p->m_right->m_parent = q;
		if (q->m_parent) {
			if (q->m_parent->m_left == q) q->m_parent->m_left = p;
			else q->m_parent->m_right = p;
		}
		if (q->m_left) q->m_left->m_parent = p;
		if (q->m_right) q->m_right->m_parent = p;
		p->m_parent = qp;
		p->m_left = ql;
		p->m_right = qr;
		p->m_height = qh;
		q->m_parent = pp;
		q->m_left = pl;
		q->m_right = pr;
		q->m_height = ph;
	}
	// exhange the places of 2 nodes.
	inline void xchNode(Node *p, Node *q) {
		if (p->m_parent == q) {
			xchWithParent(p);
			return;
		} else if (q->m_parent == p) {
			xchWithParent(q);
			return;
		} else xchFarNode(p, q);
	}

public:

	inline NodeRef find(const Key &key)
	{
		Guard g(m_lock);
		register Node *p = m_root;
		if (!p) return 0;
		while (p->m_height >= 5) {
			if (key < p->m_key) p = p->m_left;
			else if (key > p->m_key) p = p->m_right;
			else return p;
			if (key < p->m_key) p = p->m_left;
			else if (key > p->m_key) p = p->m_right;
			else return p;
		}
		do {
			if (key < p->m_key) p = p->m_left;
			else if (key > p->m_key) p = p->m_right;
			else return p;
		} while (p);
		return 0;
	}
	
	inline NodeRef findInsert(const Key &key, const Val &val, 
		                        bool override_ = false)
	{
		Guard g(m_lock);
		register Node *p = m_root;
		register Node *n = 0;
		if (p) {
			while (p->m_height >= 3) {
				if (key < p->m_key) p = p->m_left;
				else if (key > p->m_key) p = p->m_right;
				else goto found;
			}
		} else {
			m_root = new Node(key, val);
			KKNode_addRef(m_root);
			m_count++;
			return m_root;
		}
		while (true) {
			if (key < p->m_key) {
				if (p->m_left) { p = p->m_left; continue; }
				else { 
					p->m_left = n = new Node(key, val, p);
					goto inserted;
				}
			}
			else if (key > p->m_key) {
				if (p->m_right) { p = p->m_right; continue; }
				else {
					p->m_right = n = new Node(key, val, p);
					goto inserted;
				}
			}
			else goto found;
		}
found:
		if (override_) p->m_val = val;
		return p;
inserted:
		m_count++;
		KKNode_addRef(n);
		p->m_height = 2;
		if (p->m_parent) adjust<true>(p->m_parent);
		return n;
	}

	inline NodeRef insert(Node *node) 
	{
		KKNode_addRef(node);
		Guard g(m_lock);
		//validate();
		m_count++;
		if (!m_root) { m_root = node; return NodeRef(node); }
		Node *p = m_root;
		if (p) {
			while (p->m_height >= 3) { // speed up
				if (p->m_key > node->m_key) p = p->m_left;
				else p = p->m_right;
			}
			do {
				if (p->m_key > node->m_key) {
					if (p->m_left) { p = p->m_left; }
					else { p->m_left = node; node->m_parent = p; goto inserted; }
				} else {
					if (p->m_right) { p = p->m_right; }
					else { p->m_right = node; node->m_parent = p; goto inserted; }
				}
			} while (true);
		}
inserted:
		p->m_height = 2;
		if (p->m_parent) adjust<true>(p->m_parent);
		return NodeRef(node);
	}

	inline NodeRef insert(const Key &key, const Val &val) {
		return NodeRef(insert(new Node(key, val)));
	}

	inline bool remove(Node *node) 
	{
		if (!node) return false;
		Guard g(m_lock);
		if (!node->m_parent && m_root != node) return false;
		m_count--;
		if (node->m_left && node->m_right) {
			Node *p;
			if (node->m_left->m_height > node->m_right->m_height) {
				p = node->m_left;
				if (!p->m_right) {
					xchWithParent(p);
				} else {
					p = p->m_right;
					while (p->m_height >= 5) p=p->m_right->m_right;
					while (p->m_right) p = p->m_right;
					xchFarNode(node, p);
				}
			} else {
				p = node->m_right;
				if (!p->m_left) {
					xchWithParent(p);
				} else {
					p = p->m_left;
					while (p->m_height >= 5) p=p->m_left->m_left;
					while (p->m_left) p = p->m_left;
					xchFarNode(node, p);
				}
			}
			//xchNode(node, p);
			if (m_root == node) m_root = p;
		}
		Node *child = node->m_left == 0 ? node->m_right : node->m_left;
		if (node->m_parent == 0) {
			m_root = child;
			if (m_root) m_root->m_parent = 0;
			//if (node->m_right) node->m_right->m_parent = 0;
			goto final;
		} else {
			if (node->m_parent->m_left == node) node->m_parent->m_left = child;
			else node->m_parent->m_right = child;
			if (child) {
				child->m_parent = node->m_parent;
				adjust<true>(node->m_parent);
			} else {
				adjust<false>(node->m_parent);
			}
		}
final:
		if (KKConverable<Node *, KKObject *>::Val) {
			node->m_parent = node->m_left = node->m_right = 0;
		}	
		g.unlock();
		KKNode_deRef(node);
		//validate();
		return true;
	}

	void remove(const Key &key) 
	{
		Guard g(m_lock);
		NodeRef n = find(key);
		if (n) remove((Node *)n);
	}

	NodeRef first() {
		if (m_count == 0) return 0;
		Guard g(m_lock);
		Node *p = m_root;
		if (!p) return 0;
		while (p->m_left) p = p->m_left;
		return NodeRef(p);
	}
	NodeRef last() {
		if (m_count == 0) return 0;
		Guard g(m_lock);
		Node *p = m_root;
		if (!p) return 0;
		while (p->m_right) p = p->m_right;
		return NodeRef(p);
	}
	NodeRef next(Node *c) {
		Guard g(m_lock);
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
		Guard g(m_lock);
		if (!c) return 0;
		if (c->m_left == 0) {
			while (c->m_parent && c->m_parent->m_left == c) c = c->m_parent;
			return c->m_parent;
		}
		c = c->m_left;
		while (c->m_right) c = c->m_right;
		return NodeRef(c);
	}
	bool validateHeight(Node *p) {
		if (!p) return true;
		unsigned short lh = 0, rh = 0;
		if (!validateHeight(p->m_left)) return false;
		if (!validateHeight(p->m_right)) return false;
		if (p->m_left) lh = p->m_left->m_height;
		if (p->m_right) rh = p->m_right->m_height;
		if (lh > rh + 1 || rh > lh + 1) return false;
		if ((lh > rh ? lh : rh) + 1 != p->m_height) return false;
		return true;
	}
	void print_(Node *p, int tab, int tabstep) {
		if (!p) return;
		print_(p->m_right, tab + tabstep, tabstep);
		for(int i = 0; i < tab; i++) printf(" ");
		printf("%d(%d)\n", p->m_key, (int)p->m_height);
		print_(p->m_left, tab + tabstep, tabstep);
	}
	void print(int tabsize = 4) {
		print_(m_root, 0, tabsize);
	}
	bool validate(int total = -1) {
		Guard g(m_lock);
		NodeRef p = first();
		NodeRef q = next(p);
		int n = 0;
		if (p) n++;
		bool comparefailed = false;
		while (p && q) {
			n++;
			if (p->m_key > q->m_key) comparefailed = true;
			p = q;
			q = next(q);
		}
		if (total >= 0 && total != n) goto failed;
		if (n != m_count) goto failed;
		if (m_count == 0 && m_root) goto failed;
		if (m_count == 0) return true;
		if (m_root->m_parent) goto failed;
		if (comparefailed) goto failed;
		if (!validateHeight(m_root)) goto failed;
		return true;
failed:
		print_(m_root, 0, 4);
		//p = first();
		//std::stringstream ss;
		//ss << "[";
		//while(p && n >= 0) {
		//	ss << p->m_key << " ";
		//	n--;
		//	p = next(p);
		//}
		//ss << "...]";
		//std::cout << ss.str().c_str() << "\n";
		return false;
	}
};
