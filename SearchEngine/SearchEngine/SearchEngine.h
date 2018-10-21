
#pragma once

#include "utils.h"

// crawling parameters
#define DOWNLOADTIMEOUT  12000
#define PENDINGBULK      25         // push n pending urls into pending DB each time
#define MAXCRAWINGURL    100000000
#define MAXURLLEN        1024
#define MAXURLPERHOST    1000000
#define MAXHOSTTRY       10
#define THREADSTACKSIZE  (1048576/2)     // stack size of crawling thread
#define SHORTENATDOWNLOAD (true)    // shorten data on downloading

// ranking parameters
#define MINWORDPERPAGE   64
#define MAXWORDPERPAGE   2000

// temp data base for pending
#define PENDINGDBMAX     200                  // hash(host)->pendingDB index
#define PENDINGDBFILE    "dbs\\pending"
#define PENDINGRANKFILE  "pendingRank"

// main database files
#define URLFILE		     "dbs\\urls.txt"
#define CONTENTDBKEY     "dbs\\content.key"
#define CONTENTDBVAL     "dbs\\content.val"
#define SHORTENDBKEY     "dbs\\content_s.key"  // shortened content
#define SHORTENDBVAL     "dbs\\content_s.val"  // shortened content
#define FINALDICT        "dbs\\finaldict"      // main dictionary
//#define FINALDICTMICRO   "dbs\\finaldictMicro" // micro word dictionary for fast search
#define FINALDICTSMALL   "dbs\\finaldictSmall" // small word dictionary ""


#define Table16_(f, n) \
	f(0+n),f(1+n),f(2+n),f(3+n),f(4+n),f(5+n),f(6+n),f(7+n),\
	f(8+n),f(9+n),f(10+n),f(11+n),f(12+n),f(13+n),f(14+n),f(15+n)

#define Table256(f) \
	Table16_(f,0x00),Table16_(f,0x10),Table16_(f,0x20),Table16_(f,0x30),\
	Table16_(f,0x40),Table16_(f,0x50),Table16_(f,0x60),Table16_(f,0x70),\
	Table16_(f,0x80),Table16_(f,0x90),Table16_(f,0xa0),Table16_(f,0xb0),\
	Table16_(f,0xc0),Table16_(f,0xd0),Table16_(f,0xe0),Table16_(f,0xf0)

inline bool isValidURLchar(char c)
{
#define IsValidURLChar_(c) \
	((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || \
	 (c >= '0' && c <= '9') || c == ':' || c == '/' || \
	 c == '?' || c == '.' || c == '=' || c == '-' || c == '+' || \
	 c == '_' || c == '%' || c == '&' || c == ';') ? true : false

	static const bool m[256] = { Table256(IsValidURLChar_) };
	return m[(unsigned char)c];
}

inline bool isAlphabet(char c) {
#define IsAlphabet_(c) \
	((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) ? true : false
	static const bool m[256] = { Table256(IsAlphabet_) };
	return m[(unsigned char)c];
}

inline bool isSpaceTabNewLine(char c) {
	#define IsSpaceTabNewLine_(c) \
	(c == ' ' || c == '\r' || c == '\t' || c == '\n') ? true : false
	static const bool m[256] = { Table256(IsSpaceTabNewLine_) };
	return m[(unsigned char)c];
}

inline bool isValidWordChar(char c)
{
#define IsValidWordChar_(c) \
	((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || \
	 (c >= '0' && c <= '9')) ? true : false
	static const bool m[256] = { Table256(IsValidWordChar_) };
	return m[(unsigned char)c];
}

inline int getNextGBKLen(const char *data, int len) {
	if (len <= 0) return 0;
	if ((data[0] & 0x80) == 0) return 1; 
	return 2;
}

inline int getNextUTF8Len(const char *data, int len) {
	if (len <= 0) return 0;
	if ((data[0] & 0x80) == 0) return 1; 
	if ((data[0] & 0xE0) == 0xC0) {
		if (len >= 2 && (data[1] & 0xC0) == 0x80) return 2;
		return 0;
	}
	else if ((data[0] & 0xF0) == 0xE0) {
		if (len >= 3 && (data[1] & 0xC0) == 0x80 && (data[2] & 0xC0) == 0x80) return 3;
		return 0;
	}
	else if ((data[0] & 0xF8) == 0xF0) {
		if (len >= 4 && (data[1] & 0xC0) == 0x80 && (data[2] & 0xC0) == 0x80 &&
			(data[3] & 0xC0) == 0x80) return 4;
		return 0;
	}
	else if ((data[0] & 0xFC) == 0xF8) {
		if (len >= 5 && (data[1] & 0xC0) == 0x80 && (data[2] & 0xC0) == 0x80 &&
			(data[3] & 0xC0) == 0x80 && (data[4] & 0xC0) == 0x80) return 5;
		return 0;
	}
	else if ((data[0] & 0xFE) == 0xFC) {
		if (len >= 6 && (data[1] & 0xC0) == 0x80 && (data[2] & 0xC0) == 0x80 &&
			(data[3] & 0xC0) == 0x80 && (data[4] & 0xC0) == 0x80 && (data[5] & 0xC0) == 0x80) return 6;
		return 0;
	}
	return 0;
}

inline bool isValidAttributeChar(char c)
{
#define IsValidAttChar_(c) \
	((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || \
	 (c >= '0' && c <= '9') || c == '-' || c == ':') ? true : false
	static const bool m[256] = { Table256(IsValidAttChar_) };
	return m[(unsigned char)c];
}

inline char tolower(char c) {
#define ToLower_(c) \
	((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c)
	static const char m[256] = { Table256(ToLower_) };
	return m[(unsigned char)c];
}


inline KeyValDB_Key getMD5(const std::string &s) {
	MD5 md5;
	KeyValDB_Key raw;
	int offset = 0;
	if (s.length() > 4 && memcmp("www.", &(s[0]), 4) == 0) offset += 4;
	while (s[offset] == '/') offset++;
	if (offset >= s.length()) return raw;
	md5.digestMemory((BYTE *)&(s[offset]), s.length() - offset);
	memcpy(&raw, md5.digestRaw, 16);
	return raw;
}

inline const char *split(const char *url, std::string &host, int &port) {
	int len = strlen(url);
	port = 80;
	const char *end = url + len;
	if (memcmp(url, "http://", 7) == 0) url+= 7;
	else if (memcmp(url, "https://", 8)==0) {
		port = 443; url+= 8;
	}
	int hostlen = 0;
	while (url[hostlen] && url[hostlen] != '/') hostlen++;
	host = std::string(url, hostlen);
	url += hostlen;
	if (url[0] == ':') {
		url++; port = 0;
		while (url[0] >= '0' && url[0] <= '9') {
			port = port * 10 + url[0]-'0';
			url++;
		}
	}
	return url;
}

//const char *split(const std::string &url_, std::string &host, int &port) {
//	return split(url_.c_str(), host, port);
//}

template <int N>
struct FixedStr {
	short         m_length;
	unsigned char m_data[N];
	FixedStr() {
		memset(m_data, 0, N); 
		m_length = 0; 
	}
	inline FixedStr(const char *s) {
		int len = strlen(s);
		memset(m_data, 0, N);
		m_length = len;
		if (m_length > N) m_length = N;
		memcpy(m_data, s, m_length);
	}
	inline explicit FixedStr(const char *s, int len) {
		memset(m_data, 0, N);
		m_length = len;
		if (m_length > N) m_length = N;
		if (m_length) memcpy(m_data, s, m_length);
	}
	inline explicit FixedStr(const std::string &s) {
		memset(m_data, 0, N);
		int m_length = s.length();
		if (m_length > N) m_length = N;
		memcpy(m_data, s.c_str(), m_length);
	}
	inline int length() const { return m_length; }
	void calLength() {
		for (int i = 0; i < N; i++) {
			if (m_data[i] == 0) { m_length = i; return; } 
		}
		m_length = N;
	}
	bool operator < (const FixedStr &s) const { return memcmp(m_data, s.m_data, N) < 0; }
	bool operator > (const FixedStr &s) const { return memcmp(m_data, s.m_data, N) > 0; }
	bool operator <= (const FixedStr &s) const { return memcmp(m_data, s.m_data, N) <= 0; }
	bool operator >= (const FixedStr &s) const { return memcmp(m_data, s.m_data, N) >= 0; }
	bool operator == (const FixedStr &s) const { return memcmp(m_data, s.m_data, N) == 0; }
	bool operator != (const FixedStr &s) const { return memcmp(m_data, s.m_data, N) != 0; }
};

template <int N> struct KKHashFn<FixedStr<N> > {
	inline static KKHashVal hash(const FixedStr<N> &key) { 
		 KKHashVal v = 0;
		 for (int i = 0; i < N/4; i+= 4) {
			 v ^= (*(uint32_t *)&(key.m_data[i])) * KKGolden64;
			 v ^= (v >> 17);
		 }
		 return v;
	}
};

template <int N, bool Phrase_>
struct DictWord_ { // a word or phrase stored in final dictionary

#pragma pack(push, 1)
	enum _ { MaxWordLen = N, Phrase = Phrase_};
	enum NoInit_ { NoInit };
	unsigned char m_word[MaxWordLen];
	float         m_rank;
	uint32_t      m_pageIndex;
	uint16_t      m_position; // position of the word/phrase in the page
#pragma pack(pop)

private:
	void normalizeWord() {
		if (!Phrase) {
			for (int i = 0; i < N; i++) {
				if (m_word[i] == ' ') {
					while (i < N) m_word[i++] = 0;
					break;
				}
			}
		}
	}
public:
	inline DictWord_(NoInit_ _) { } 
	inline DictWord_() { memset(this, 0, sizeof(*this)); }
	inline DictWord_(const DictWord_ &w) { memcpy(this, &w, sizeof(*this)); }
	template <int N2, bool P2> inline DictWord_(const DictWord_<N2, P2> &w) {
		if (N == N2) { 
			memcpy(this, &w, sizeof(*this)); 
			if (Phrase != P2) normalizeWord(); 
			return; 
		}
		if (N > N2) {
			memset(m_word, 0, sizeof(m_word));
			memcpy(m_word, w.m_word, N2);
			normalizeWord();
		} else {
			memset(m_word, 0, sizeof(m_word));
			int len = 0, p = 0;
			while (p <= N) {
				if (!Phrase && w.m_word[p] == ' ') { len = p; break; }
				if (w.m_word[p] == 0) { len = p; break; }
				if (w.m_word[p] & 0x80) {
					int l2 = ::getNextUTF8Len((const char*)&(w.m_word[p]), 6);
					if (l2 > 0 && p + l2 <= N) {
						len = p + l2; p += l2; continue;
					}
					else { len = p; break; }
				} else {
					if (!isValidWordChar(w.m_word[p])) len = p;
					p++;
				}
			}
			if (len) memcpy(m_word, w.m_word, len);
		}
		m_rank = w.m_rank;
		m_pageIndex = w.m_pageIndex;
		m_position = w.m_position;
	}
	inline DictWord_(const char *s, int l, float r, uint32_t pageIndex, int position) {
		memset(this, 0, sizeof(*this));
		if (l) {
			if (l > sizeof(m_word)) l = sizeof(m_word);
			memcpy(m_word, s, l);
		}
		m_rank = r;
		m_pageIndex = pageIndex;
		m_position = position;
	}
	size_t wordLen() const {
		if (m_word[sizeof(m_word)-1]) return sizeof(m_word);
		else return strlen((const char *)m_word);
	}
	int cmp(const DictWord_ &w) const {
		int r = memcmp(m_word, w.m_word, sizeof(m_word));
		if (r) return r;
		if (m_rank > w.m_rank) return 1;
		if (m_rank < w.m_rank) return -1;
		//if (m_rank != w.m_rank) 
		//	return m_rank > w.m_rank ? 1 : -1;
		if (m_pageIndex > w.m_pageIndex) return 1;
		if (m_pageIndex < w.m_pageIndex) return -1;
		return 0;
	}
	int cmp(const std::string &s) const {
		int len = (int)s.length();
		if (!len) {
			return m_word[0] ? 1 : 0;
		}
		if (!Phrase) len++;
		if (len >= sizeof(m_word)) {
			return memcmp(m_word, s.c_str(), sizeof(m_word));
		}
		int r = memcmp(m_word, s.c_str(), len);
		if (!Phrase) {
			return r;
		}
		if (r) return r;

		bool isEnglish = true;
		for (int i = 0; i < len; i++) {
			if (((unsigned char)s[i]) > 127) { isEnglish = false; break; }
		}
		bool partialCompare = true;
		if (isEnglish) {
			unsigned char nextChar = m_word[len];
			if ((nextChar >= 'a' && nextChar <= 'z') ||
				(nextChar >= 'A' && nextChar <= 'Z')) partialCompare = false;
		}
		return partialCompare ? 0 : 1;
	}
	bool operator < (const DictWord_ &w) const { return cmp(w) < 0; }
	bool operator <=(const DictWord_ &w) const { return cmp(w) <= 0; }
	bool operator > (const DictWord_ &w) const { return cmp(w) > 0; }
	bool operator >=(const DictWord_ &w) const { return cmp(w) >= 0; }
	bool operator ==(const DictWord_ &w) const { return cmp(w) == 0; }
	bool operator !=(const DictWord_ &w) const { return cmp(w) != 0; }
};

typedef DictWord_<8,  false>  DictWordSmall; // small word dictionary
typedef DictWord_<32, true>   DictWord;      // main word & phrase dictionary

template <typename T, int Max>
struct KKFixedArray {
	uint32_t m_count;
	T     m_data[Max];
	inline KKFixedArray(int len = 0): m_count(len) { }
	inline void push_back(const T &t) {
		if (m_count < Max) m_data[m_count++] = t;
	}
	inline size_t size() const { return m_count; }
	inline T &operator[] (size_t i) { return m_data[i]; }
	inline const T &operator[] (size_t i) const { return m_data[i]; }
};

struct RankArray {
	float rank[2];
	inline RankArray(float v = 0) { rank[0] = 0; rank[1] = v; }
};

struct Model
{
	typedef KeyValDB<KKLock, KKObject, RankArray> DB;

	struct PendDB {
		KKRef<DB>					  m_db;
		std::string                   m_urlBuf[PENDINGBULK];
		KeyValDB_Key                  m_urlmd5[PENDINGBULK];
		KKAtomic<int>                 m_bufCount;
		KKAtomic<int>                 m_insertCount;
		KKLock                        m_lock;

		inline size_t count() { return m_db ? m_db->count() : 0; }
		bool insert(const std::string &u, const KeyValDB_Key &md5) {
			KKLockGuard<KKLock> g(m_lock);
			if (m_bufCount >= PENDINGBULK) {
				std::vector<char> urls;
				urls.resize((MAXURLLEN + 4 + sizeof(KeyValDB_Key)) * PENDINGBULK);
				int len = 0;
				for (int i = 0; i < m_bufCount; i++) {
					memcpy(&(urls[len]), m_urlBuf[i].c_str(), 
							m_urlBuf[i].length());
					len += m_urlBuf[i].length();
					urls[len++] = (char)0; // delimeter
					memcpy(&(urls[len]), &(m_urlmd5[i]), sizeof(KeyValDB_Key));
					len += sizeof(KeyValDB_Key);
				}
				KeyValDB_Key key(m_insertCount++);
				m_db->add(key, &(urls[0]), len);
				m_bufCount = 0;
			}
			m_urlBuf[m_bufCount] = u;
			m_urlmd5[m_bufCount] = md5;
			m_bufCount++;
			return true;
		}

		int getURLs(std::vector<char> &urls_md5s, 
			        std::string &url, KeyValDB_Key &md5) {
			KKLockGuard<KKLock> g(m_lock);
			KeyValDB_Key key;
			if (!m_db || !m_db->randKey(&key)) {
				if (m_bufCount) { 
					// get from uncommited pending buffer
					url = m_urlBuf[m_bufCount-1];
					md5 = m_urlmd5[m_bufCount-1];
					m_bufCount--;
					return 1;
				} else return 0;
			}
			m_db->get(key, urls_md5s);
			m_db->remove(key);
			return PENDINGBULK;
		}
	};

	KKLock                        m_lock;

	typedef KKHash<KeyValDB_Key>  URLSet_;
	typedef KKHash<KeyValDB_Key, KKNul, KKNul, KKLock, 
		           KKHeapBase<sizeof(URLSet_::Node)> > URLSet;

	URLSet                        m_pendingURLs;
	URLSet                        m_processingUrls; // processing + bad

	KKHash<KeyValDB_Key, int>     m_hostsCount;
	KKHash<KeyValDB_Key, int>     m_badHosts;
	KKHash<KeyValDB_Key, in_addr> m_okHosts;
	FILE                          *m_urlFile;

	KKRef<DB>				      m_contentDB;

	int                           m_nThreads;
	size_t                        m_maxCrawlingURLs;
	int                           m_pendDBTotal;
	PendDB                        m_pendDB[PENDINGDBMAX];

	double                        m_connTime;
	double                        m_recvTime;
	double                        m_parseTime;
	long long                     m_nSuccess;

	KKAtomic<uint64_t>            m_sentBytes;
	KKAtomic<uint64_t>            m_rcvdBytes;
	KKAtomic<uint64_t>            m_rcvdHttpsBytes;
	KKAtomic<uint64_t>            m_compressedBytes;

	std::string                   m_lastURL;
	bool                          m_printState;
	DWORD                         m_lastPrintTime;
	size_t                        m_freeSystemRAM;

	// url logging
	char                          m_pendingURLLogBuf[1048576];
	int                           m_pendingURLLogBufLen;

	KKAtomic<int>                 m_nIdleThreads;

	typedef KKHash<FixedStr<8>, KKNul, KKNul, KKNoLock> FilterWordHash;
	FilterWordHash                m_filterWords;
	KKHash<uint32_t>              m_filterUTF8Char;

	int                           m_maxWordPerPage;
	Model() {
		m_connTime = m_recvTime = m_parseTime = 0;
		m_nSuccess = 0;
		m_lastPrintTime = 0;
		m_pendingURLLogBufLen = 0;
		m_sentBytes = m_rcvdBytes = 0;
		m_printState = true;

		m_freeSystemRAM = Utils::nFreeRAM();

		m_nThreads = 0;
		m_maxCrawlingURLs = MAXCRAWINGURL;
		m_pendDBTotal = PENDINGDBMAX;
		m_maxWordPerPage = MAXWORDPERPAGE;

		m_filterWords.insert("www");
		m_filterWords.insert("com");
		m_filterWords.insert("a");
		m_filterWords.insert("of");
		m_filterWords.insert("the");
		m_filterWords.insert("for");

		m_filterUTF8Char.insert(0x8cbcef); // ,
		m_filterUTF8Char.insert(0x8280e3); // .
		m_filterUTF8Char.insert(0x9fbcef); // ?
		m_filterUTF8Char.insert(0x9abcef); // :
		m_filterUTF8Char.insert(0x9c80e2); // "
		m_filterUTF8Char.insert(0x9d80e2); // "
		m_filterUTF8Char.insert(0x81bcef); // !
	}

	inline bool filterWord(const char *w, int len) {
		if (!len) return true;
		if (w[0] & 0x80) {
			if (len >= 3) {
				uint32_t v = (uint32_t)(uint8_t)w[0] + (((uint32_t)(uint8_t)w[1]) << 8) + 
					          (((uint32_t)(uint8_t)w[2]) << 16);
				if (m_filterUTF8Char.find(v)) return true;
			}
			return false;
		} else if (len <= 4) {
			FilterWordHash::Node *n = m_filterWords.find(FixedStr<8>(w, len));
			if (n) return true;
			return false;
		}
		return false;
	}

	bool initDB4Crawling(int nthreads) {
		if (m_contentDB) return false;
		m_nThreads = nthreads;
		if (m_nThreads < m_pendDBTotal) m_pendDBTotal = m_nThreads;
		m_contentDB = new DB(CONTENTDBKEY, CONTENTDBVAL, 
			                 false, false, true, 1048576 * 1, 1048576 * 8);
		for (int i = 0; i < m_pendDBTotal; i++) {
			char keyPath[4096], valPath[4096];
			sprintf(keyPath, "%s_%d.key", PENDINGDBFILE, i);
			sprintf(valPath, "%s_%d.val", PENDINGDBFILE, i);
			m_pendDB[i].m_db = new DB(keyPath, valPath, 
				                       true, false, false, 65536, 1048576 / 2);
		}
		return true;
	}
	bool initDB4Ranking() {
		if (m_contentDB) return false;
		FILE *f1 = fopen(SHORTENDBKEY, "rb");
		if (f1) { 
			fclose(f1);
			m_contentDB = new DB(SHORTENDBKEY, SHORTENDBVAL, false, false, true);
		} else {
			m_contentDB = new DB(CONTENTDBKEY, CONTENTDBVAL, false, false, true);
		}
		m_contentDB->createReadBuf(1048576 * 1, 1048576 * 16);
		return true;
	}
	bool initDB4Searching() {
		if (m_contentDB) return false;
		m_contentDB = new DB(CONTENTDBKEY, CONTENTDBVAL, false, false, true);
		return true;
	}
	void print() {
		if (!m_printState) return;
		if (::timeGetTime() - m_lastPrintTime < 200) return;
		int len = 0;
		m_freeSystemRAM = Utils::nFreeRAM();
		KKLockGuard<KKLock> g(m_lock);
		if (::timeGetTime() - m_lastPrintTime < 200) return;
		m_lastPrintTime = ::timeGetTime();
		std::vector<char> text;
		text.resize(8192);
		len += sprintf(&(text[0]),
			   "hosts:%d(bad:%d ok:%d) "
			   "url:(contentDB:%d processing+bad:%d ok:%d pend:%d) "
			   "conn:%.0lfms recv:%.0lfms parse:%.0lfms "
			   "sentBytes:%lld rcvdBytes:(http %lld, https %lld) "
			   "compressed:%lld idleThreads:%d freeRAM:%dMB\n",
			   m_hostsCount.count(),  m_badHosts.count(), m_okHosts.count(),
			   m_contentDB ? m_contentDB->count() : 0, m_processingUrls.count(), m_nSuccess, m_pendingURLs.count(), 
			   m_nSuccess ? m_connTime / (m_nSuccess) : 0,
			   m_nSuccess ? m_recvTime / (m_nSuccess) : 0,
			   m_nSuccess ? m_parseTime / (m_nSuccess) : 0,
			   (long long)m_sentBytes, 
			   (long long)m_rcvdBytes, (long long)m_rcvdHttpsBytes,
			   (long long)m_compressedBytes, (int)m_nIdleThreads,
			   (long long)m_freeSystemRAM / 1048576);
		len += sprintf(&(text[len]), "PendDBs:[");
		for (int i = 0; i < m_pendDBTotal; i++) {
			len += sprintf(&(text[len]), "%6llu ", m_pendDB[i].count()); 
		}
		len += sprintf(&(text[len]), "]\n");
		len += sprintf(&(text[len]), "lastURL:%.*s%s\n", 
			           m_lastURL.length() > 160 ? 160 : m_lastURL.length(), m_lastURL.c_str(),
					   m_lastURL.length() > 160 ? "..." : "");
		::fwrite(&(text[0]), 1, len, stdout);
	}

	// hash for avoiding frequecy websit access
	inline static uint64_t hostCollisionHash(const std::string &host) {
		int i = host.length() - 1;
		int remaindots = 1;
		while (i >= 0) {
			if (host[i] == '.') {
				if (!remaindots) {
					if (i + 4 <= host.length() && 
						(!memicmp(&(host[i]), ".com", 4)||
						 !memicmp(&(host[i]), ".org", 4))) {
						i--; continue;
					} else {
						i++; break;
					}
				}
				remaindots--;
			}
			i--;
		}
		if (i < 0) i = 0;
		std::string indexStr = (const char *)&(host[i]);
		KeyValDB_Key md5 = getMD5(indexStr);
		return (md5.m_k[0] ^ md5.m_k[1]);
	}
private:
	inline bool filterURL(const std::string &u, int *pendDBIndex) {
		int len = u.length();
		if (len < 4 || len > MAXURLLEN) return false;

		if (len > 4) {
			if (memcmp(&(u.c_str()[len-3]), ".js", 3) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".css", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".jpg", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".png", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".zip", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".gif", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".pdf", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".exe", 4) == 0) return false;
			if (memcmp(&(u.c_str()[len-4]), ".rar", 4) == 0) return false;
		}

		std::string host;
		int port;
		split(u.c_str(), host, port);
		if (host.length() <= 2) return false;
		if (host.length() > 3) {
			if (memcmp(&(host.c_str()[host.length()-3]), ".jp", 3) == 0) return false;
		}
		KeyValDB_Key hostmd5 = getMD5(host);
		if (KKHash<KeyValDB_Key, int>::NodeRef node = m_hostsCount.find(hostmd5)) {
			if (node->m_val >= MAXURLPERHOST) return false;
			node->m_val++;
		} else {
			m_hostsCount.insert(hostmd5, 1);
			//printf("+host %s\n", host.c_str());
		}

		// pendDB index
		*pendDBIndex = hostCollisionHash(host) % m_pendDBTotal;
		return true;
	}
public:
	static bool truncateURL(std::string &u) {
		int p = 0, len = u.length();
		if (len < 3) return false;
		if (u.length() >= 7 && memcmp(u.c_str(), "https", 5) == 0) {
			if (u[5] != ':') return false;
			while (len > 0 && u[len-1] == '/') len--;
			if (!len) u = "";
			else u = u.substr(0, len);
			return true; // don't truncate https URLs
		}
		if (u.length() >= 7 && memcmp(u.c_str(), "http", 4) == 0) {
			p = 4;
			if (u[p] != ':') return false;
			p++;
		}
		while (u.length() > p + 1 && u[p] == '/') p++;
		while (len > 0 && u[len-1] == '/') len--;
		if (len - p < 3) return false; 
		if (p || len != u.length()) u = u.substr(p, len - p);
		return true;
	}
	inline bool pushPending(std::string u, bool writeTextFile) 
	{
		int pendDBInd = 0;
		if (m_pendingURLs.count() + m_processingUrls.count()
			+ (m_contentDB ? m_contentDB->count() : 0) >= m_maxCrawlingURLs) return false;
		if (!truncateURL(u)) return false;
		if (!filterURL(u, &pendDBInd)) return false;
		KeyValDB_Key md5 = getMD5(u);
		KKLockGuard<KKLock> g(m_lock);
		if (!m_processingUrls.find(md5) && !m_pendingURLs.find(md5) && !m_contentDB->exist(md5)) {
			if (writeTextFile && m_urlFile) {
				if (m_pendingURLLogBufLen + u.length() + 10 >= sizeof(m_pendingURLLogBuf)) {
					fwrite(m_pendingURLLogBuf, m_pendingURLLogBufLen, 1, m_urlFile);
					m_pendingURLLogBufLen = 0;
				}
				memcpy(&(m_pendingURLLogBuf[m_pendingURLLogBufLen]), u.c_str(), u.length());
				m_pendingURLLogBufLen += u.length();
				m_pendingURLLogBuf[m_pendingURLLogBufLen++] = '\n';
			}
			if (m_pendDB[pendDBInd].m_db) {
				m_pendDB[pendDBInd].insert(u, md5);
				m_pendingURLs.insert(md5);
			}
			return true;
		}
		return false;
	}
};

void getSelfURLfromContent(const std::vector<char> &content, 
						   std::string &selfURL, bool convert = false);
void getTitlefromContent(const std::vector<char> &content, 
						 std::string &title, bool convert = false);
extern Model g_model;
