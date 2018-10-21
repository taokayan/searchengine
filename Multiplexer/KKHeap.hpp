#pragma once

#include "KKBase.h"
#include "KKLockGuard.h"

template <typename ID, size_t ItemSize, size_t nItem>
class KKFixHeap {
private:
	char              *m_start;
	char              *m_end;
	KKAtomic<char *>   m_head;
public:
	static KKFixHeap m_instance;
    inline static KKFixHeap *getInstance() { return &m_instance; }
private:
	KKFixHeap(const KKFixHeap &);
	KKFixHeap &operator=(const KKFixHeap &);
	inline char * &next(char *head) {
		return *(char **)head;
	}
private:
	inline KKFixHeap() {
		size_t nitem = nItem;
		size_t itemsize = (ItemSize + sizeof(void *) - 1) 
			              / sizeof(void *) * sizeof(void *);
		m_start = (char *)::malloc(itemsize * nitem);
		if (!m_start) return;
		m_end = m_start + itemsize * nitem;
		m_head = m_start;
		for (int i = 0; i < nitem - 1; i++) {
			*(char **)(m_head + i * itemsize) = (m_head + (i + 1)*itemsize);
		}
		*(char **)(m_head + (nitem - 1) * itemsize) = 0;
	}
public:
	inline void *alloc(size_t itemSize) {
retry:
		volatile char *head = m_head;
		if (!head) {
			return (void *)::malloc(itemSize);
		}
		volatile char *next = *(char **)head;
		if (m_head.cmpXch((char *)next, (char *)head) == head) {
			return (void *)head;
		}
		goto retry;
	}
	inline void free(void *p_) {
		char *p = (char *)p_;
		if (p >= m_start && p < m_end) {
retry:
			volatile char *head = m_head;
			*(char **)p = (char *)head;
			if (m_head.cmpXch((char *)p, (char *)head) != head) goto retry;
		} else {
			::free(p_);
		}
	}
};

template <typename ID, size_t ItemSize, size_t nItem>
KKFixHeap<ID, ItemSize, nItem> KKFixHeap<ID, ItemSize, nItem>::m_instance;

template <typename ID, size_t ItemSize, size_t nItem>
class KKFixHeapBase : public KKObject {
public:
	inline void *operator new(size_t size) {
		return KKFixHeap<ID, ItemSize, nItem>::getInstance()->alloc(size);
	}
	inline void operator delete(void *p) {
		KKFixHeap<ID, ItemSize, nItem>::getInstance()->free(p);
	}
};


template <size_t Size_, size_t AllocSize_ = 65504>
class KKHeap {
	KKAssert(AllocSize_ < 65536);
	enum _ { Align = 2, 
		     Size = (Size_ + Align - 1) / Align * Align,
			 AllocSize = (AllocSize_ + Align - 1) / Align * Align };
	struct Node;
	struct Page;
#pragma pack(push, 2)
	struct Node {
		friend struct Page;
		//Page *      m_page;
		uint16_t      m_pageOffset;
		union {
			char    m_data[Size];
			Node    *m_next;
		} m_u;
		inline void page(Page *p) {
			//m_page = p;
			m_pageOffset = (uint16_t)((char *)this - (char *)p);
		}
		inline Page *page() {
			return (Page *)((char *)this - m_pageOffset);
		}
	};
#pragma pack(pop)
	enum __ { NodePerPage = (AllocSize - sizeof(void *)*3 - sizeof(int) *2) / (sizeof(Node)) };
	struct Page {
		enum _ { Magic = 0xf01cad97 };
		Node  m_node[NodePerPage];
		Node  *m_firstNode;
		Page  *m_next;
		Page  *m_prev;
		int   m_nAvail;
		int   m_magic;
		Page() {
			m_next = 0;
			m_prev = 0;
			m_nAvail = NodePerPage;
			m_magic = Magic;
			m_firstNode = &m_node[0];
			for(int i = 0; i < NodePerPage - 1; i++) {
				m_node[i].m_u.m_next = &(m_node[i + 1]);
			}
			m_node[NodePerPage - 1].m_u.m_next = 0;
		}
		void *alloc() {
			if (!m_nAvail) return 0;
			m_nAvail--;
			Node *n = m_firstNode;
			m_firstNode = m_firstNode->m_u.m_next;
			n->page(this);
			return n->m_u.m_data;
		}
		void free(void *data) {
			Node *n = (Node *)((char *)data - offsetof(Node, m_u.m_data));
			n->m_u.m_next = m_firstNode;
			m_firstNode = n;
			m_nAvail++;
		}
	};
	KKAssert(sizeof(Page) <= AllocSize);
	KKAssert(NodePerPage >= 2);

	Page   *m_firstPage;
	int    m_nAlloc;
	int    m_nDelete;
	KKLock m_lock;
public:
	public:
	static KKHeap * const m_instance;
    inline static KKHeap *getInstance() { return m_instance; }
private:
	KKHeap() {
		m_firstPage = 0;
		m_nAlloc = m_nDelete = 0;
	}
public:
	~KKHeap() {
		if (m_nAlloc != m_nDelete) {
			printf("KKHeap<%d> memory leak....\n", (int)Size_);
		}
	}
	void *alloc() {
		KKLockGuard<KKLock> g(m_lock);
		if (m_firstPage == 0) {
			m_firstPage = new Page();
			m_nAlloc++;
		}
		void *p = m_firstPage->alloc();
		if (!m_firstPage->m_nAvail) {
			if (m_firstPage->m_next) m_firstPage->m_next->m_prev = 0;
			m_firstPage = m_firstPage->m_next;
		}
		return p;
	}
	void free(void *data) {
		KKLockGuard<KKLock> g(m_lock);
		Node *n = (Node *)((char *)data - offsetof(Node, m_u.m_data));
		Page *p = n->page();
		if (p->m_magic != Page::Magic) {
			printf("KKHeap magic check failed...\n");
			return;
		}
		p->free(data);
		if (p->m_nAvail == 1) {
			p->m_next = m_firstPage;
			if (m_firstPage) m_firstPage->m_prev = p;
			m_firstPage = p;
			return;
		} else if (p->m_nAvail == NodePerPage) {
			if (p->m_prev) p->m_prev->m_next = p->m_next;
			if (p->m_next) p->m_next->m_prev = p->m_prev;
			if (m_firstPage == p) m_firstPage = p->m_next;
			delete p;
			m_nDelete++;
		}
	}
};

// just let it memory leak 
template <size_t Size_, size_t AllocSize_ >
KKHeap<Size_,AllocSize_> * const KKHeap<Size_,AllocSize_>::m_instance = new KKHeap<Size_,AllocSize_>();

template <size_t ItemSize>
struct KKHeapBase {
public:
	enum _ { AlignSize = ItemSize <= 8 ? 8:
		                 ItemSize <= 16 ? 16:
						 ItemSize <= 24 ? 24:
						 ItemSize <= 32 ? 32:
						 ItemSize <= 40 ? 40: // special tuning
						 ItemSize <= 64 ? 64:
						 ItemSize <= 96 ? 96:
						 ItemSize <= 128 ? 128:
						 ItemSize <= 192 ? 192:
						 ItemSize <= 256 ? 256: 1 };
	inline void *operator new(size_t size) {
		if (AlignSize > 1) 
			return KKHeap<AlignSize>::getInstance()->alloc();
		else 
			return ::malloc(size);
	}
	inline void operator delete(void *p) {
		if (AlignSize > 1) 
			KKHeap<AlignSize>::getInstance()->free(p);
		else 
			::free(p);
	}
};

