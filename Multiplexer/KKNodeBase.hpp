
#pragma once
#include "KKBase.h"

template <typename Node>
inline void KKNode_addRef(Node *n) {
	if (KKConverable<Node *, KKObject *>::Val) {
		((KKObject *)n)->addRef();
	}
}

template <typename Node>
inline void KKNode_deRef(Node *n) {
	if (KKConverable<Node *, KKObject *>::Val) {
		((KKObject *)n)->deRef();
	} else {
		delete n;
	}
}

template <typename Node, bool isKKObject>
struct KKNodeRetType_ {
	KKAssert(!isKKObject);
	typedef Node *T;
};
template <typename Node>
struct KKNodeRetType_<Node, true> {
	typedef KKLocalRef<Node> T;
};
template <typename Node>
struct KKNodeRetType {
	typedef typename KKNodeRetType_<Node, KKConverable<Node *, KKObject *>::Val>::T T;
};
