// KeyValueDB.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <unordered_map>
#include <map>
#include <Windows.h>
#include "..\..\Multiplexer\KKAVLTree.hpp"
#include "..\..\Multiplexer\KKHash.hpp"
#include "..\..\Multiplexer\KKHeap.hpp"

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable: 4996)

#pragma pack(push, 1)

void *KeyValDB_compress_alloc();
void KeyValDB_compress_free(void *p);
int KeyValDB_compress(const unsigned char *in, int len, 
					  std::vector<unsigned char> *out, void *memory);
int KeyValDB_decompress(unsigned char *in, int len, 
						std::vector<char> *decode);
int KeyValDB_compressTest(const char *data, size_t len, const char *fout);

struct KeyValDB_Key {
  unsigned long long m_k[2];
  inline KeyValDB_Key(unsigned long long low = 0) {
    m_k[0] = low, m_k[1] = 0;
  }
  inline KeyValDB_Key(unsigned long long low, 
              unsigned long long high) {
                m_k[0] = low, m_k[1] = high; 
  }
  inline bool operator < (const KeyValDB_Key &k) const {
    if (m_k[1] != k.m_k[1]) return m_k[1] < k.m_k[1];
    else return m_k[0] < k.m_k[0];
  }
  inline bool operator > (const KeyValDB_Key &k) const {
    if (m_k[1] != k.m_k[1]) return m_k[1] > k.m_k[1];
    else return m_k[0] > k.m_k[0];
  }
  inline bool operator ==(const KeyValDB_Key &k) const {
    return memcmp(this, &k, sizeof(*this)) == 0;
  }
  inline bool operator !=(const KeyValDB_Key &k) const {
    return memcmp(this, &k, sizeof(*this)) != 0;
  }
};

template <> struct KKHashFn<KeyValDB_Key> {
	inline static KKHashVal hash(const KeyValDB_Key &key) { 
		 return (key.m_k[0] * KKGolden64) ^ 
			    ((key.m_k[1] * KKGolden64) >> 32) ^
				((key.m_k[1] * KKGolden64) << 32);
	}
};

typedef uint64_t KeyValDB_Offset;

#pragma pack(pop)

//namespace std {
//template<> struct hash<KeyValDB_Key>
//		: public _Bitwise_hash<KeyValDB_Key>
//	{ };
//}

template <typename Lock = KKLock, typename Base = KKNul,
          typename TempData = KKNul>
class KeyValDB : public Base {
public:
  typedef KeyValDB_Key Key;

  struct K2OHashVal {
	  KeyValDB_Offset m_offset;
	  TempData        m_tempData;
	  inline K2OHashVal() { }
	  inline K2OHashVal(KeyValDB_Offset o) : m_offset(o) { }
  };

  typedef typename KKHash<Key, K2OHashVal, KKNul, KKNoLock, KKNul> Key2ValOffset_;
  typedef typename KKHash<Key, K2OHashVal, KKNul, KKNoLock, 
	             KKHeapBase<sizeof(typename Key2ValOffset_::Node)> > Key2ValOffset;

  typedef typename Key2ValOffset::Node    K2ONode;
  typedef typename Key2ValOffset::NodeRef K2ONodeRef;
//  KKAssert(sizeof(Key2ValOffset::Node) <= 40);

  // keyNode on disk
  struct KeyNode {
    Key       m_key;
    long long m_valFileOffset;
  };
  struct ValNodeHeader {
    int       m_valLen;
  };
  struct CompressRAM {
	KKLock     m_lock;
	void      *m_mem;
	inline CompressRAM(): m_mem(0) { }
	inline void alloc() {
		if (!m_mem) m_mem = KeyValDB_compress_alloc();
	}
	inline ~CompressRAM() { 
		if (m_mem) KeyValDB_compress_free(m_mem); 
	}
  };

private:
  Key2ValOffset       m_k2vOffset;
  long long           m_keyFileLen;
  long long           m_valFileLen;
  FILE                *m_keyFile;
  FILE                *m_valFile;
  Lock                m_lock;
  bool                m_compress;
  bool                m_autoFlush;
  bool                m_removeOnExit;
  bool                m_readOnly;

  std::string         m_keypath, m_valpath;

  enum _ { NCompRAM = 16 };
  KKAtomic<int>       m_compRAMIndex;
  CompressRAM         m_compRAM[NCompRAM];

  // append buffer
  std::vector<char>   m_keyBuf;
  std::vector<char>   m_valBuf;
  int                 m_keyBufLen;
  int                 m_valBufLen;

  // read buffer
  std::vector<char>   m_keyReadBuf;
  std::vector<char>   m_valReadBuf;
  long long           m_keyReadBufOffset;
  long long           m_valReadBufOffset;
  long long           m_keyReadBufLen;
  long long           m_valReadBufLen;

  uint64_t            m_ioCount;

public:
  ~KeyValDB() {
	KKLockGuard<Lock> g(m_lock);
    if (m_keyFile) fclose(m_keyFile);
    if (m_valFile) fclose(m_valFile);
    m_keyFile = m_valFile = 0;
	if (m_removeOnExit) {
		::remove(m_keypath.c_str());
		::remove(m_valpath.c_str());
	}
  }
  KeyValDB(const char *keyFilePath, const char *valFilePath, 
	       bool clear = false, bool autoFlush = true, 
		   bool compress = false, 
		   size_t keyAppendBuf = 256 * 1024, // 256k
		   size_t valAppendBuf = 1048576 * 2, // 2M
           bool removeOnExit = false, bool readOnly = false)
    : m_k2vOffset(8, 1)  {
	m_keypath = keyFilePath;
	m_valpath = valFilePath;
	m_valBuf.resize(valAppendBuf);
	m_keyBufLen = m_valBufLen = 0;
    m_keyFileLen = m_valFileLen = 0;
	m_compress = compress;
	m_autoFlush = autoFlush;
	m_removeOnExit = removeOnExit;
	m_keyReadBufOffset = m_valReadBufOffset = 0;
	m_keyReadBufLen = m_valReadBufLen = 0;
	m_ioCount = 0;
	m_readOnly = readOnly;
	if (m_compress) {
		for (int i = 0; i < sizeof(m_compRAM) / sizeof(m_compRAM[0]); i++) {
			m_compRAM[i].alloc();
		}
	}

	if (clear && removeOnExit) {
		m_keyFile = 0;
	} else {
		if (clear) {
			FILE *f = ::fopen(keyFilePath, "w");
			if (f) fclose(f);
			f = ::fopen(valFilePath, "w");
			if (f) fclose(f);
		}
		m_keyFile = ::fopen(keyFilePath, "r+b");
		if (!m_keyFile) {
			m_keyFile = ::fopen(keyFilePath, "w+b");
		}
		else {
			::_fseeki64(m_keyFile, 0, SEEK_END);
			long long s = ::_ftelli64(m_keyFile);
			if (s > 0)
				m_keyFileLen = s / sizeof(KeyNode) * sizeof(KeyNode);
		}
		if (!m_keyFile) return;
	}
	if (keyAppendBuf) m_keyBuf.resize(keyAppendBuf);

    m_valFile = ::fopen(valFilePath, "r+b");
    if (!m_valFile) {
      m_valFile = ::fopen(valFilePath, "w+b");
    } else {
      ::_fseeki64(m_valFile, 0, SEEK_END);
      long long s = ::_ftelli64(m_valFile);
      if (s > 0)
        m_valFileLen = (s + 7 ) / 8 * 8;
    }
    if (!m_valFile) return;

    bool needseek = true;
    for (long long i = 0; i < m_keyFileLen; i+= sizeof(KeyNode)) {
      KeyNode knode;
      if (needseek) ::_fseeki64(m_keyFile, i, SEEK_SET);
      needseek = false;
      if (fread(&knode, sizeof(KeyNode), 1, m_keyFile) == 1) {
        if (knode.m_valFileOffset < 0) {
          m_k2vOffset.remove(knode.m_key);
          if (knode.m_valFileOffset != -1) {
            printf("error at key %llu\n", knode.m_key.m_k[0]);
          }
        }
        else {
          Key2ValOffset::NodeRef n = m_k2vOffset.find(knode.m_key);
          if (n) n->m_val = knode.m_valFileOffset;
          else m_k2vOffset.insert(knode.m_key, knode.m_valFileOffset);
        }
      } else needseek = true;
    }
  }

private:
  void flushBuffer(bool flushFile = true) {
	  if (m_readOnly) return;
	  if (m_valBufLen) {
		  ::_fseeki64(m_valFile, m_valFileLen, SEEK_SET);
		  fwrite(&(m_valBuf[0]), m_valBufLen, 1, m_valFile);
		  m_valFileLen += m_valBufLen;
		  m_valBufLen = 0;
		  if (flushFile) { fflush(m_valFile); m_ioCount += 3; }
		  else m_ioCount += 2;
	  }
	  if (m_keyBufLen) {
		  if (m_keyFile) {
			  ::_fseeki64(m_keyFile, m_keyFileLen, SEEK_SET);
			  fwrite(&(m_keyBuf[0]), m_keyBufLen, 1, m_keyFile);
		  }
		  m_keyFileLen += m_keyBufLen;
		  m_keyBufLen = 0;
		  if (flushFile && m_keyFile) { 
			  fflush(m_keyFile); m_ioCount += 3; 
		  }
		  else m_ioCount += 2;
	  }
  }
  void pushKeyNode_(KeyNode &knode) {
      Key2ValOffset::NodeRef n = m_k2vOffset.find(knode.m_key);
      if (n) n->m_val = knode.m_valFileOffset;
      else m_k2vOffset.insert(knode.m_key, knode.m_valFileOffset);

	  if (!m_autoFlush) {
		if (sizeof(KeyNode) + m_keyBufLen >= m_keyBuf.size()) {
			flushBuffer();
		}
		memcpy(&(m_keyBuf[m_keyBufLen]), &knode, sizeof(KeyNode));
		m_keyBufLen += sizeof(KeyNode);
		return;
	  } else {
		flushBuffer();
		if (m_keyFile) ::_fseeki64(m_keyFile, m_keyFileLen, SEEK_SET);
		if (m_keyFile) fwrite(&knode, sizeof(KeyNode), 1, m_keyFile);
		m_keyFileLen += sizeof(KeyNode);
		if (m_keyFile) fflush(m_keyFile);
		m_ioCount += 3;
	  }
  }
  long long pushValNode_(const char *data, int len) {
	  if (!m_valFile) return -1;
	  if (!m_autoFlush) {
		  if (len + sizeof(len) + m_valBufLen >= m_valBuf.size()) {
			  flushBuffer();
		  }
	      if (len + sizeof(len) + m_valBufLen < m_valBuf.size()) {
			  long long o = m_valFileLen + m_valBufLen;
			  memcpy(&(m_valBuf[m_valBufLen]), &len, sizeof(len));
			  m_valBufLen += sizeof(len);
			  memcpy(&(m_valBuf[m_valBufLen]), data, len);
			  m_valBufLen += len;
			  return o;
		  }
	  }
	  flushBuffer();
      long long o = m_valFileLen;
      ::_fseeki64(m_valFile, m_valFileLen, SEEK_SET);
      ::fwrite(&len, sizeof(len), 1, m_valFile);
      m_valFileLen += sizeof(len);
      if (len) {
        ::fwrite(data, 1, len, m_valFile);
        m_valFileLen += (len + 7)/8*8;
      }
      fflush(m_valFile);
	  m_ioCount += 4;
      return o;
  }

  size_t seekReadKey(void *buf, size_t size, size_t count, size_t offset) {
	if (!(size * count)) return 0;
	if (!m_keyFile) return 0;
	if (!m_keyReadBuf.size() || m_keyReadBuf.size() < (size *count)) { 
		m_ioCount += 2;
		::_fseeki64(m_keyFile, offset, SEEK_SET);
		return ::fread(buf, size, count, m_keyFile);
	}
	if (!((int64_t)offset >= m_keyReadBufOffset && 
		  (int64_t)(offset + size * count) <= m_keyReadBufOffset + m_keyReadBufLen)) {
		m_ioCount += 2;
		::_fseeki64(m_keyFile, offset, SEEK_SET);
		m_keyReadBufOffset = offset;
		m_keyReadBufLen = ::fread(&(m_keyReadBuf[0]), 1, m_keyReadBuf.size(), m_keyFile);
	}
	if ((int64_t)offset >= m_keyReadBufOffset && 
		(int64_t)(offset + size * count) <= m_keyReadBufOffset + m_keyReadBufLen) {
		size_t bufOffset = offset - m_keyReadBufOffset;
		memcpy(buf, &(m_keyReadBuf[bufOffset]), size * count);
		return count;
	}
	return 0;
  }
  size_t seekReadVal(void *buf, size_t size, size_t count, size_t offset) {
	if (!(size * count)) return 0;
	if (!m_valReadBuf.size() || m_valReadBuf.size() < (size *count)) { 
		m_ioCount += 2;
		::_fseeki64(m_valFile, offset, SEEK_SET);
		return ::fread(buf, size, count, m_valFile);
	}
	if (!((int64_t)offset >= m_valReadBufOffset && 
		  (int64_t)(offset + size * count) <= m_valReadBufOffset + m_valReadBufLen)) {
		m_ioCount += 2;
		::_fseeki64(m_valFile, offset, SEEK_SET);
		m_valReadBufOffset = offset;
		m_valReadBufLen = ::fread(&(m_valReadBuf[0]), 1, m_valReadBuf.size(), m_valFile);
	}
	if ((int64_t)offset >= m_valReadBufOffset && 
		(int64_t)(offset + size * count) <= m_valReadBufOffset + m_valReadBufLen) {
		size_t bufOffset = offset - m_valReadBufOffset;
		memcpy(buf, &(m_valReadBuf[bufOffset]), size * count);
		return count;
	}
	return 0;
  }

public:
  void createReadBuf(size_t keyBufSize, size_t valBufSize) {
	  KKLockGuard<Lock> g(m_lock);
	  m_keyReadBufLen = 0;
	  m_valReadBufLen = 0;
	  m_keyReadBuf.resize(keyBufSize);
	  m_valReadBuf.resize(valBufSize);
  }
  void flush() {
	  if (m_readOnly) return;
	  KKLockGuard<Lock> g(m_lock);
	  flushBuffer(false);
	  if (m_valFile) { fflush(m_valFile); m_ioCount++; }
	  if (m_keyFile) { fflush(m_keyFile); m_ioCount++; }
  }
  inline size_t count() const {
	  return m_k2vOffset.count();
  }
  inline size_t ioCount() const { 
	  return m_ioCount;
  }
  inline long long keyFileLen() const {
    return m_keyFileLen + m_keyBufLen;
  }
  inline long long keyFileMaxIndex() const {
	  return (m_keyFileLen + m_keyBufLen) / sizeof(KeyNode);
  }
  inline long long valFileLen() const {
    return m_valFileLen + m_valBufLen;
  }
  bool seek(long long keyFileIndex, Key *key) {
	  KKLockGuard<Lock> g(m_lock);
	  if (!m_keyFile) return false;
	  if (!m_readOnly) flush();
	  long long keyfileoffset = keyFileIndex * sizeof(KeyNode);
	  if (keyfileoffset >= m_keyFileLen) return false;
	  KeyNode keynode;
	  if (seekReadKey(&keynode, sizeof(KeyNode), 1, keyfileoffset)) {
		  *key = keynode.m_key;
		  return true;
	  }
	  return false;
  }

  int add(const Key &k, const char *data, int len) {
	if (m_readOnly) return 0;
	std::vector<unsigned char> encoded;
	if (m_compress) {
		encoded.reserve(len/2);
		int i = (m_compRAMIndex++) % NCompRAM;
		KKLockGuard<KKLock> g_(m_compRAM[i].m_lock);
		len = KeyValDB_compress((const unsigned char*)data, len, 
			                    &encoded, m_compRAM[i].m_mem);
	}
	KKLockGuard<Lock> g(m_lock);
	long long newOffset;
	if (m_compress) {
		newOffset = pushValNode_((const char*)&(encoded[0]), len);
	}
	else {
		newOffset = pushValNode_(data, len);
	}
	KeyNode knode;
	knode.m_key = k;
	knode.m_valFileOffset = newOffset;
	pushKeyNode_(knode);
	return len;
  }

  bool exist(const Key &k) {
	KKLockGuard<Lock> g(m_lock);
    return m_k2vOffset.find(k);
  }
  bool exist_nolock(const Key &k) {
	  return m_k2vOffset.find(k);
  }
  int get(const Key &k, std::vector<char> &data) {
	KKLockGuard<Lock> g(m_lock);
    Key2ValOffset::NodeRef node = m_k2vOffset.find(k);
    if (!node) return -1;
    long long offset = node->m_val.m_offset;
    if (offset < 0 || !m_valFile) return -1;
	if (!m_autoFlush && offset >= m_valFileLen) {
		flush();
	}
   // ::_fseeki64(m_valFile, offset, SEEK_SET);
    int len = 0;
    //fread(&len, sizeof(int), 1, m_valFile);
	seekReadVal(&len, sizeof(int), 1, offset);
    if (len < 0 || offset + len > m_valFileLen) {
      return -1;
    }
    if (len > 0) {
		if (m_compress) {
			std::vector<char> encoded;
			encoded.resize(len);
			//fread(&(data[0]), 1, len, m_valFile);
			int r = seekReadVal(&(encoded[0]), 1, len, offset + sizeof(int));
			if (r <= 0) r = 0;
			if (r) data.reserve(r * 2);
			len = KeyValDB_decompress((unsigned char*)&(encoded[0]), 
				                      r, &data);
		} else {
			data.resize(len);
			//fread(&(data[0]), 1, len, m_valFile);
			int r = seekReadVal(&(data[0]), 1, len, offset + sizeof(int));
			if (r <= 0) r = 0;
			if (r < len) { len = r; data.resize(r); }
		}
	}
    return len;
  }
  void remove(const Key &k) {
	if (m_readOnly) return;
	KKLockGuard<Lock> g(m_lock);
    if (!exist(k)) return;
    KeyNode knode;
    knode.m_key = k;
    knode.m_valFileOffset = -1;
    pushKeyNode_(knode);
    m_k2vOffset.remove(k);
  }
  bool randKey(KeyValDB_Key *key) {
	KKLockGuard<Lock> g(m_lock);
	Key2ValOffset::NodeRef node = m_k2vOffset.randNode();
	if (!node) return false;
	*key = node->m_key;
	return true;
  }
  bool firstKey(KeyValDB_Key *key) {
	KKLockGuard<Lock> g(m_lock);
	Key2ValOffset::NodeRef node = m_k2vOffset.first();
	if (!node) return false;
	*key = node->m_key;
	return true;
  }
  typename Key2ValOffset::NodeRef firstNode() {
	KKLockGuard<Lock> g(m_lock);
	return m_k2vOffset.first();
  }
  typename Key2ValOffset::NodeRef nextNode(typename Key2ValOffset::NodeRef node) {
	KKLockGuard<Lock> g(m_lock);
	return m_k2vOffset.next(node);
  }
  TempData *getTempData(const Key &k) {
	KKLockGuard<Lock> g(m_lock);
    Key2ValOffset::NodeRef node = m_k2vOffset.find(k);
	if (!node) return 0;
	return &(node->m_val.m_tempData);
  }
};

//int kvtest()
//{
//  printf("sizeof(KeyValDB::Key2ValOffset::Node) = %d\n", 
//         sizeof(KeyValDB<KKLock, KKNul>::Key2ValOffset::Node));
//
//  KeyValDB<KKLock, KKNul> db("1.key", "1.val");
//
//  char cmd[4096], val[4096];
//  std::vector<char> data;
//  unsigned long long key;
//
//  printf("# of keys = %llu, keyFileLen = %llu, valFileLen = %llu\n", 
//    db.count(), db.keyFileLen(), db.valFileLen());
//
//  while (true) {
//    printf(">");
//    if (scanf("%s%llu", cmd, &key) == 2) {
//      if (strcmp(cmd, "add") == 0) {
//        val[0] = 0;
//        scanf("%s", val);
//        db.add(key, val, strlen(val) + 1);
//      } else if (strcmp(cmd, "get") == 0) {
//        if (db.get(key, data)) {
//          data.push_back(0);
//          printf("%s\n", &(data[0]));
//        } else {
//          printf("(null)\n");
//        }
//      } else if (strcmp(cmd, "del") == 0) {
//        db.remove(key);
//      } else if (strcmp(cmd, "rand") == 0) {
//        long long start = key, total = 0;
//        scanf("%llu", &total);
//        long long t = ::GetTickCount();
//        for (int i = 0; i < total; i++) {
//          char str[128];
//          sprintf(str, "this is a %d string of key %d", rand(), start+i);
//          db.add(start+i, str, strlen(str) + 1);
//        }
//        t = ::GetTickCount() - t;
//        printf("rand insert %lld records, time %dms\n", total, t);
//      } else {
//        printf("error. add/get/del key [value]\n");
//      }
//    }
//  }
//	return 0;
//}