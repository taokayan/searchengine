// SearchEngine.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <queue>
#include <string.h>
//#include <mmsystem.h>

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)

#include "md5.h"
#include "..\..\Multiplexer\KKLock.h"
#include "..\..\Multiplexer\KKTask.h"
#include "..\..\Multiplexer\KKHeap.hpp"
#include "..\..\Multiplexer\KKHash.hpp"
#include "..\..\Multiplexer\KKQueue.hpp"

#include "utils.h"
#include "KeyValueDB.hpp"
#include "SearchEngine.h"
#include "ExternalSorter.hpp"
#include "KKSocket.hpp"
#include "httpDownload.h"
#include "winHttpDownload.h"
#include "BufferReader.h"

#pragma comment(lib, "winmm.lib") 
#pragma comment(lib, "Winhttp.lib")

Model g_model;

// get URLs from contents, mainly for crawling & ranking
int getURLs(std::vector<char> &data, const std::string &host, bool host_https,
			bool pushPending = true,
			std::set<std::string> *outURLs = 0) {
	static const char pat0[] = "href=\"//";
	static const char pat1[] = "href=\"http://";
	static const char pat1s[] = "href=\"https://";
	static const char pat2[] = "href=\"/";
	int count = 0;
	int indexEnd = data.size() - 21;
	if (indexEnd <= 0) return 0;
	std::string u;
	u.reserve(2048);
	KKHash<KeyValDB_Key, KKNul, KKNul, KKNoLock> seenUrl;
	for (int i = 0; i < indexEnd; i++) {
		bool https = false;
		int offset = Utils::findchar(&(data[i]), indexEnd - i, 'h'); // short cut
		if (offset < 0) break;
		i += offset;
		if (*(uint32_t *)&(data[i]) != *(uint32_t *)"href") continue;
		u.clear();
		int p = i;
		bool samehost = false;
		if (memcmp(&(data[p]), pat0, sizeof(pat0)-1) == 0) {
			p += sizeof(pat0)-1;
			while (p < data.size() && data[p] != '\"') {
				if (!samehost && (data[p] == '/' || data[p] == ':' || data[p] == '?')) {
					if (u == host) samehost = true;
				}
				u += data[p++];
			}
		} else if (memcmp(&(data[p]), pat1, sizeof(pat1)-1) == 0) {
			p += sizeof(pat1)-1;
			while (p < data.size() && data[p] != '\"') {
				if (!samehost && (data[p] == '/' || data[p] == ':' || data[p] == '?')) {
					if (u == host) samehost = true;
				}
				u += data[p++];
			}
		} else if (memcmp(&(data[p]), pat1s, sizeof(pat1s)-1) == 0) {
			https = true;
			p += sizeof(pat1s)-1;
			while (p < data.size() && data[p] != '\"') {
				if (!samehost && (data[p] == '/' || data[p] == ':' || data[p] == '?')) {
					if (u == host) samehost = true;
				}
				u += data[p++];
			}
		} else if (memcmp(&(data[p]), pat2, sizeof(pat2)-1) == 0) {
			p += sizeof(pat2)-1;
			samehost = true;
			u = host; u += '/';
			while (p < data.size() && data[p] != '\"') {
				u += data[p++];
			}
		} else {
			continue;
		}
		if (!samehost && u == host) samehost = true;
		if (u.length()) {
			if ((samehost && host_https) || 
				(!samehost && https)) u = "https://" + u;
			KeyValDB_Key md5 = getMD5(u);
			if (!seenUrl.find(md5)) {
				seenUrl.insert(md5);
				if (pushPending) g_model.pushPending(u, true);
				if (outURLs && Model::truncateURL(u)) {
					outURLs->insert(u);
				}
				count++;
			}
		}
		i = p;
	}
	return count;
}

void removeComment(const std::vector<char> &data,
				   std::vector<char> &result) 
{
	if (!data.size()) return;				
	result.reserve(data.size());
	int i = 0;
	while (i < (int)data.size() - 4) {
		if (memcmp(&(data[i]), "<!--", 4) == 0) {
			i+=4;
			while (i < (int)data.size() - 3 && memcmp(&(data[i]), "-->", 3)) i++;
			i+=3;
		} else {
			result.push_back(data[i]);
			i++;
		}
	}
	while (i < data.size()) result.push_back(data[i++]);
	return;
}

int shortenData(const std::vector<char> &data_,
			 std::vector<char> &shortenData)
{
	// ignore tags
	static const char script_str[] = {"<script"};
	static const char script_str2[] = {"</script"};
	static const char style_str[] = {"<style"};
	static const char style_str2[] = {"</style"};

	// preserve tags
	static const char meta_str[] = {"<meta"};
	static const char meta_str2[] = {">"};
	static const char img_str[] = {"<img"};
	static const char img_str2[] = {">"};

	// attribute that we only cares...
	static const std::string attList[] = { "href" };

	if (!data_.size()) return 0;

	std::vector<char> data;
	removeComment(data_, data);
	shortenData.reserve(data.size() / 2 + 8);

#define IGNORE_(begin_str, end_str) \
	if (i <= data.size() - (sizeof(begin_str)-1) && memcmp(&(data[i]), begin_str, sizeof(begin_str)-1)==0) { \
		i += sizeof(begin_str)-1;\
		while (i < data.size() - sizeof(end_str) && \
				memcmp(&(data[i]), end_str, sizeof(end_str)-1) != 0) i++;\
		i += sizeof(end_str)-1;\
		while (i < data.size() && data[i] != '>') i++;\
		i++; continue;\
	}

#define PRESERVE_(begin_str, end_str) \
	if (i <= data.size() - (sizeof(begin_str)-1) && memcmp(&(data[i]), begin_str, sizeof(begin_str)-1)==0) { \
		int oldi = i;\
		i += sizeof(begin_str)-1;\
		while (i < data.size() - sizeof(end_str) && \
				memcmp(&(data[i]), end_str, sizeof(end_str)-1) != 0) i++;\
		i += sizeof(end_str)-1;\
		while (i < data.size() && data[i] != '>') i++;\
		i++; if (i >= data.size()) i = data.size(); \
		while (oldi < i) shortenData.push_back(data[oldi++]);\
		continue;\
	}

	int i;
	for (i = 0; i < (int)data.size() - 10;) {
		const char *p = &(data[i]); // test only
		if (data[i] == '<') {
			if (data[i+1] == '!') {
				i += 2;
				while (i < data.size() && data[i] != '>') i++;
				i++; continue;
			}
			IGNORE_(script_str, script_str2)
			IGNORE_(style_str, style_str2)
			PRESERVE_(meta_str, meta_str2)
			PRESERVE_(img_str, img_str2)

#undef IGNORE_
#undef PRESERVE_
			shortenData.push_back(data[i]);
			i++;
			int beginAtt = -1, endAtt = -1, beginAttVal = -1, endAttVal = -1;
			while (i < data.size() && data[i] != '>') {
				if (beginAtt == -1) {
					if (data[i] == ' ') {
						beginAtt = 0;
					}
				} else if (beginAtt == 0 && isAlphabet(data[i])) {
					beginAtt = i;
					endAtt = i + 1;
					while (endAtt < data.size() && (isValidAttributeChar(data[endAtt]))) endAtt++;
					endAttVal = beginAttVal = endAtt;
					while (beginAttVal < data.size() && isSpaceTabNewLine(data[beginAttVal])) beginAttVal++;
					if (beginAttVal < data.size() && data[beginAttVal] == '=') {
						beginAttVal++;
						endAttVal = beginAttVal;
						while (endAttVal < data.size() && isSpaceTabNewLine(data[endAttVal])) endAttVal++;
						if (endAttVal < data.size() && data[endAttVal] == '\"') {
							endAttVal++;
							while (endAttVal < data.size() && data[endAttVal] != '\"') endAttVal++;
							endAttVal++;
						} else {
							while (endAttVal < data.size() && !isSpaceTabNewLine(data[endAttVal]) &&
								   data[endAttVal] != '/' && data[endAttVal] != '>') endAttVal++;
						}
					}
					std::string att = std::string(&(data[beginAtt]), endAtt - beginAtt);
					bool ignore = true;
					for (int j = 0; j < sizeof(attList) / sizeof(attList[0]); j++)  {
						if (att == attList[j]) { ignore = false;  break; }
					}
					if (!ignore) {
						if (endAttVal > data.size()) endAttVal = data.size();
						while (i < data.size() && i < endAttVal) {
							shortenData.push_back(data[i]); i++;
						}
					} 
					beginAtt = -1;
					i = endAttVal;
					if (i < data.size() && data[i-1] == ' ') {
						while (i < data.size() && isSpaceTabNewLine(data[i])) i++; // skip double spaces
					}
					continue;
				}
				shortenData.push_back(data[i]);
				i++;
			}
			if (i >= data.size()) goto end;
			continue;
		}
		shortenData.push_back(data[i]);
		i++;
	}
	while (i < (int)data.size()) shortenData.push_back(data[i++]);
end:
	return shortenData.size();
}

struct GetWordsParam : public KKObject {
	typedef KKHash<FixedStr<DictWord::MaxWordLen>, 
		           DictWord, KKNul, KKNoLock>           Words;
//	typedef KKHash<FixedStr<DictWord::MaxWordLen>, 
//		           DictWord, KKNul, KKNoLock, 
//				   KKHeapBase<sizeof(Words_::Node)> >   Words;

	typedef KKHash<FixedStr<DictWordSmall::MaxWordLen>, 
		           DictWordSmall, KKNul, KKNoLock>      SmallWords;
//	typedef KKHash<FixedStr<DictWordSmall::MaxWordLen>, 
//		           DictWordSmall, KKNul, KKNoLock,
//				   KKHeapBase<sizeof(SmallWords_::Node)> > SmallWords;
	uint32_t          m_pageIndex;   // in 
	std::vector<char> m_pageData;    // in
	float             m_pageRank;    // in
	Words             m_words;       // out
	SmallWords        m_smallWords;  // out
	int               m_wordCount;   // out
	int               m_unicodeWordCount;  // out
	inline GetWordsParam() : KKObject(), 
		m_words(8, 1), m_smallWords(6, 1) { }
};

// get phrase from contents, for reverse indexing
int getWords(uint32_t pageIndex, float pageRank, std::vector<char> &data_,
			 GetWordsParam::Words &words, 
			 int &unicodeWordCount) // key = word, val = weight
{
	typedef GetWordsParam::Words Words;

	static const char title_str[] = {"<title"};
	static const char title_str2[] = {"</title"};
	static const char body_str[] = {"<body"};
	static const char body_str2[] = {"</body"};

	// ignore tags
	static const char script_str[] = {"<script"};
	static const char script_str2[] = {"</script"};
	static const char style_str[] = {"<style"};
	static const char style_str2[] = {"</style"};

	// others
	static const char bold_str[] = {"<b>"};
	static const char bold_str2[] = {"</b>"};
	static const char header_strs[][5] = { "<h1>", "<h2>", "<h3>", "<h4>", "<h5>", "<h6>" };
	static const char header_strs2[][6] = { "</h1>", "</h2>", "</h3>", "</h4>", "</h5>", "</h6>" };

	// weights
	static const float urlWeight = 40.0, titleWeight = 100.0, bodyWeight = 1.0; // base weights
	static const float boldMult = 2.0, hdrMult[] = {4.0, 1.5, 1.4, 1.3, 1.0, 0.8}; // amplifiers
	static const float capitalFactor = 4.0;
	static const float defactorMult= 0.999; // decreasing on each word found
	float defactor = 1.0f;

	unicodeWordCount = 0;
	if (!data_.size()) return 0;
	
	std::vector<char> data2;
	if (!SHORTENATDOWNLOAD) {
		removeComment(data_, data2);
	}
	std::vector<char> &data = SHORTENATDOWNLOAD ? data_ : data2;

	enum State_ { Start = 0, URL, Header, Title, Pad2, Body, Tail, NStates };
	const static bool validStates[] = { 0, 1, 0, 1, 0, 1, 0 };
	KKAssert(sizeof(validStates) / sizeof(validStates[0]) == NStates);

	int state = Start; // 0 -> title (1) -> 2 -> body(3) -> 4
	char word[DictWord::MaxWordLen]; // UTF8 word
	int wl = 0, count = 0, bold = 0, hdrIndex = -1;
	double totalRank = 0;

	int position = 1;
	for (int i = 0; i < data.size() - 10;) {
		int old_i = i;
		//const char *p = &(data[i]); // test only
		if (state == Start && data[i] == '\"') {
			state = URL; wl = 0; i++;
		}
		else if (state < Header && data[i] == '\"') {
			state = Header; wl = 0; i++; continue;
		}
		else if (data[i] == '<') { 
			if (data[i+1] == '!') {
				i += 2;
				while (i < data.size() && data[i] != '>') i++;
				i++;
				continue;
			}
			else if (state < Title && memcmp(&(data[i]), title_str, sizeof(title_str)-1)== 0) {
				state = Title;  wl = 0; // start new word
			}
			else if (state < Pad2 && memcmp(&(data[i]), title_str2, sizeof(title_str2)-1)== 0) {
				state = Pad2; 
			}
			else if (state < Body && memcmp(&(data[i]), body_str, sizeof(body_str)-1)== 0) {
				state = Body;  wl = 0; // start new word
			}
			else if (state < Tail && memcmp(&(data[i]), body_str2, sizeof(body_str2)-1)== 0) {
				state = Tail; goto end;
			}
			else if (memcmp(&(data[i]), script_str, sizeof(script_str)-1)==0) { // ignore script
				i += sizeof(script_str)-1;
				while (i < data.size() - 10 && 
					   memcmp(&(data[i]), script_str2, sizeof(script_str2)-1) != 0) i++;
				i += sizeof(script_str2)-1;
				while (i < data.size() && data[i] != '>') i++;
				i++; continue;
			}
			else if (memcmp(&(data[i]), style_str, sizeof(style_str)-1)==0) { // ignore style
				i += sizeof(style_str)-1;
				while (i < data.size() - 10 && 
					   memcmp(&(data[i]), style_str2, sizeof(style_str2)-1) != 0) i++;
				i += sizeof(script_str2)-1;
				while (i < data.size() && data[i] != '>') i++;
				i++; continue;
			}
			else if (memcmp(&(data[i]), bold_str, sizeof(bold_str)-1)==0) {
				bold = 1;
			}
			else if  (memcmp(&(data[i]), bold_str2, sizeof(bold_str2)-1)==0) {
				bold = 0;
			} else {
				for (unsigned j = 0; j < sizeof(header_strs) / sizeof(header_strs[0]); j++) {
					if (memcmp(&(data[i]), header_strs[j], sizeof(header_strs[j])-1) == 0) {
						hdrIndex = j; break; 
					}
				}
				for (unsigned j = 0; j < sizeof(header_strs2) / sizeof(header_strs2[0]); j++) {
					if (memcmp(&(data[i]), header_strs2[j], sizeof(header_strs2[j])-1) == 0) {
						hdrIndex = -1; break; 
					}
				}
			}
			i++;
			if (i < data.size()) {
				int ii = Utils::findchar(&(data[i]), data.size() - i, '>');
				if (ii < 0) goto end;
				i += ii + 1;
			} else goto end;
			wl = 0; continue; // start new word
		}
		if (!validStates[state]) { i++; continue; }
		//if (state != Body && state != URL && state != Title) { i++; continue; }

		int nextWord = 0;
		wl = 0;
		bool isEnglish;
		bool capital = false;
		if ((data[i] & 0x80) == 0) { // English / control chars
			if (!isValidWordChar(data[i])) { i++; continue; }
			isEnglish = true;
			do {
				word[wl] = tolower(data[i]);
				if (word[wl] != data[i]) capital = true;
				wl++, i++;
again:
				if (wl >= sizeof(word)) break;
				if (i >= data.size()) break;
				if (!isValidWordChar(data[i])) { // build up phrase
					if (data[i] == ' ' || data[i] == '.') {
						while (i < data.size() && (data[i] == ' ' || data[i] == '.')) i++;
						word[wl++] = data[i - 1];
						if (!nextWord) nextWord = i;
						goto again;
					}
					if (!nextWord) nextWord = i;
					break;
				}
			} while (true);
			while (wl && (word[wl - 1] == ' ' || word[wl - 1] == '.')) wl--; // trimp space
			if (!nextWord) nextWord = old_i + wl;
		}
		else { // other language char
			isEnglish = false;
			int wcharLen = getNextUTF8Len(&(data[i]), (int)data.size() - i);
			if (wcharLen <= 0) { i++; continue; }
			nextWord = i + wcharLen;
			do {
				if (wl + wcharLen >= sizeof(word)) break;
				if (wcharLen == 1) 
					word[wl++] = tolower(data[i++]); 
				else {
				  for (int j = 0; j < wcharLen; j++) 
					  word[wl++] = data[i++];
				}
				wcharLen = getNextUTF8Len(&(data[i]), (int)data.size() - i);
				if (wcharLen == 1 && !isValidWordChar(data[i])) break;
			} while (wcharLen > 0);
		}

		//std::string sword = std::string((const char *)word, wl);
		FixedStr<DictWord::MaxWordLen> sword_((const char *)word, wl);
		if (wl && !g_model.filterWord(word, wl)) {
			float rank = state == URL ? urlWeight :
				         state == Title ? titleWeight : 
						                  bodyWeight;
			if (hdrIndex >= 0) rank *= hdrMult[hdrIndex];
			if (bold) rank *= boldMult;
			if (capital) rank *= capitalFactor;
			rank *= defactor;
			defactor *= defactorMult;
			Words::NodeRef node = words.find(sword_);
			if (!node) {
				totalRank += rank;
				DictWord dictWord(word, wl, rank, pageIndex, position);
				words.insert(sword_, dictWord); count++;
				if (!isEnglish) unicodeWordCount++;
				if (count >= g_model.m_maxWordPerPage) goto end;
			} else if (rank > node->m_val.m_rank) {
				totalRank = rank - node->m_val.m_rank;
				node->m_val.m_rank = rank;
				node->m_val.m_position = position;
			}
		}
		position++;
		if (position >= 65535) goto end; // position overflow
		if (nextWord > old_i) i = nextWord;
		else i = old_i + 1;
	}
end:
	if (totalRank) { // normalize
		double fac = pageRank / totalRank;
		//if (count && count < MAXWORDPERPAGE) { // not exceed maximum
		//	totalRank *= MAXWORDPERPAGE / count;
		//}
		for (Words::NodeRef node = words.first(); 
			 node; node = words.next(node)) {
			node->m_val.m_rank *= fac;
		}
	}
	return count;
}

struct GetWords_CommonParas {
	typedef ExternalSorter<DictWord, KKObject> ExSorter;
	typedef ExternalSorter<DictWordSmall, KKObject> ExSorterSmall;

	KKAtomic<size_t> m_wordCount, m_unicodeWordCount, m_maxWordOfPage;
	KKAtomic<size_t> m_smallWordCount;
	KKAtomic<size_t> m_getWordFinishCount;
	
	KKLocalRef<ExSorter> m_finalDict;
	KKLocalRef<ExSorterSmall> m_finalDictSmall;
};

void getWords_helper(KKQueue<KKLocalRef<GetWordsParam> > *inqueue, 
					 KKQueue<KKLocalRef<GetWordsParam> > *outqueue, 
					 KKSemaphore *sem, GetWords_CommonParas *cp) {
	typedef KKQueue<KKLocalRef<GetWordsParam> > Queue;
	while (sem->wait(INT_MAX)) {
		KKLocalRef<Queue::Node> job = inqueue->pop();
		if (!job) return;
		Queue::Node *jobNode = job.get();
		GetWordsParam *param = jobNode->get();
		param->m_wordCount = getWords(param->m_pageIndex, param->m_pageRank,
			param->m_pageData, param->m_words, param->m_unicodeWordCount);
		for (GetWordsParam::Words::NodeRef w = param->m_words.first();
			 w; w = param->m_words.next(w)) {
			DictWordSmall dws = w->m_val;
			FixedStr<DictWordSmall::MaxWordLen> sw((const char*)dws.m_word, 
				                                   dws.wordLen());
			GetWordsParam::SmallWords::Node *node;
			if (sw.length()) {
				node = param->m_smallWords.find(sw);
				if (!node) param->m_smallWords.insert(sw, dws);
				else {
					if (node->m_val.m_rank < dws.m_rank) node->m_val = dws;
				}
			}
		}
		//outqueue->push(param);
		{
			// push to external sorter
			cp->m_wordCount += param->m_wordCount;
			cp->m_unicodeWordCount += param->m_unicodeWordCount;
			if (param->m_wordCount > cp->m_maxWordOfPage) 
				cp->m_maxWordOfPage = param->m_wordCount;
			cp->m_smallWordCount += param->m_smallWords.count();
			DictWord buf[1024];
			int count = 0;
			for (GetWordsParam::Words::NodeRef n1 = param->m_words.first();
				n1; n1 = param->m_words.next(n1)) {
				buf[count++] = n1->m_val;
				if (count == sizeof(buf)/sizeof(buf[0])) {
					cp->m_finalDict->push(buf, count);
					count = 0;
				}
			}
			cp->m_finalDict->push(buf, count);
			count = 0;
			DictWordSmall buf2[1024];
			for (GetWordsParam::SmallWords::NodeRef n1 = param->m_smallWords.first();
				n1; n1 = param->m_smallWords.next(n1)) {
				buf2[count++] = n1->m_val;
				if (count == sizeof(buf2)/sizeof(buf2[0])) {
					cp->m_finalDictSmall->push(buf2, count);
					count = 0;
				}
			}
			cp->m_finalDictSmall->push(buf2, count);
			cp->m_getWordFinishCount++;
		}
		//KKLock_Yield();
	}
}

// download from url
void download(std::string url_, KeyValDB_Key md5, std::string &host)
{
	int port = 80;
	const char *url = split(url_.c_str(), host, port);

	KeyValDB_Key hostmd5 = getMD5(host);
	bool okHost = false;
	in_addr ipv4;

	KKHash<KeyValDB_Key, int>::NodeRef badhostNode = 0;
	{ // bad host check
		KKLockGuard<KKLock> g(g_model.m_lock);
		KKHash<KeyValDB_Key, in_addr>::NodeRef hostNode = g_model.m_okHosts.find(hostmd5);
		if (hostNode) {
			okHost = true;
			ipv4 = hostNode->m_val;
		} else {
			badhostNode = g_model.m_badHosts.find(hostmd5);
			if (badhostNode && badhostNode->m_val > MAXHOSTTRY && (rand() % 10) > 0) {
				return;
			}
		}
	}

	DWORD t0 = ::timeGetTime(), t1 = t0;
	std::vector<char> data;
	bool ok = true;
	bool https = (port == 443);
	if (!https) {
		KKSocket client;
		if (okHost) {
			if (!client.connect(ipv4, port)) goto connect_fail;
		} else {
			if (!client.connect(host.c_str(), port, &ipv4))
				goto connect_fail;
			g_model.m_okHosts.findInsert(hostmd5, ipv4);
		}
		if (badhostNode) g_model.m_badHosts.remove(badhostNode);
		// successfully connect
		t1 = ::timeGetTime();
		if (httpDownload(client, url, host.c_str(), data, &g_model)) {
			g_model.m_rcvdBytes += data.size();
		}
		client.close();
	} else {
		ok = downloadEx(host.c_str(), port, url, data, 256 * 1024);
		if (ok) {
			if (badhostNode) g_model.m_badHosts.remove(badhostNode);
			g_model.m_rcvdHttpsBytes += data.size();
		}
	}

	//if (data.size()) fwrite(&(data[0]), 1, data.size(), stdout);
	//printf("\n");
	if (ok && data.size()) {
		DWORD t2 = ::timeGetTime();
		std::vector<char> *pData = &data;
		std::vector<char> sData;
		if (SHORTENATDOWNLOAD) {
			::shortenData(data, sData);
			pData = &sData;
			data.clear();
		}
		if (pData->size()) {
			std::vector<char> data2;
			data2.resize(url_.length() + (*pData).size() + 2);
			int p = 0;
			data2[p++] = '"';
			memcpy(&(data2[p]), url_.c_str(), url_.length()); p+= url_.length();
			data2[p++] = '"';
			memcpy(&(data2[p]), &((*pData)[0]), (*pData).size());
			g_model.m_compressedBytes += g_model.m_contentDB->add(md5, &(data2[0]), data2.size());
			g_model.m_processingUrls.remove(md5);
			getURLs((*pData), host, https);
			DWORD t3 = ::timeGetTime();
			//printf("[INFO] %d bytes from %s (con:%dms,recv:%dms,parse:%dms)\n", 
			//	   data.size(), url_.c_str(), t1-t0, t2-t1, t3-t2);
		
			KKLockGuard<KKLock> g(g_model.m_lock);
			g_model.onDownloaded(host);
			g_model.m_connTime += t1 - t0;
			g_model.m_recvTime += t2 - t1;
			g_model.m_parseTime += t3 - t2;
			g_model.m_nSuccess++;
			g_model.m_lastURL = url_;
			g.unlock();


			//if (t3 - t0 < 1000) {
			//	::Sleep(1000 - (t3 - t0)); // minimum 1s per download
			//}
		}
	}
	DWORD t4 = ::timeGetTime();
	return;

connect_fail:
	if (!okHost) {
		KKLockGuard<KKLock> g(g_model.m_lock);
		if (badhostNode) badhostNode->m_val++;
		else g_model.m_badHosts.findInsert(hostmd5, 1, true);
	}
	//printf("[WARNING] failed to connect %s\n", url_.c_str());
	return;
}

void crawlingThread(bool *stop, int nThreads, int threadInd) {
	int pendDBInd = threadInd % g_model.m_pendDBTotal;
	bool idle = false;
	int nextDownloadTime = ::timeGetTime();
	::Sleep(rand() % (DOWNLOADTIMEOUT + 1)); // randomize start time
	KKHash<uint32_t> recentHostHashs;
	std::vector<char> urls;
	urls.reserve(102400);
	while (!*stop) {
		std::string url, host;
		KeyValDB_Key md5;
		KeyValDB_Key pendingInd;
		Model::PendDB *pendDB = &(g_model.m_pendDB[pendDBInd]);
		int dt = nextDownloadTime - ::timeGetTime();
		if (dt > 0) ::Sleep(dt < DDOSDELAY ? dt : DDOSDELAY);
		int r = pendDB->getURLs(urls, url, md5);
		if (r == 0) {
			// no URL to download
			g_model.m_nIdleThreads++;
			idle = true;
			::Sleep(rand() % 500);
			g_model.m_nIdleThreads--;
			continue;
		}
		else if (r == 1 && url.length()) {
			idle = false;
			g_model.m_processingUrls.insert(md5);
			g_model.m_pendingURLs.remove(md5);
			if (url.length()) {
				nextDownloadTime = ::timeGetTime() + DDOSDELAY;
				download(url, md5, host);
			}
			continue;
		}
		else if (urls.size()) {
			KKHash<KeyValDB_Key, int> hostLastTime;
			idle = false;
			int offset = 0, processedCount = 0;
			int failedremove = 0;
			std::vector<std::string> urllist;
			std::vector<KeyValDB_Key> urlMD5list;
			std::vector<KeyValDB_Key> hostlist;
			urllist.reserve(100);
			while (offset < urls.size() - 1) {
				int len = 1, port;
				while (offset + len < urls.size() && urls[offset + len] != 0) len++;
				if (offset + len + sizeof(KeyValDB_Key) >= urls.size()) break;
				urls[offset + len] = 0;
				url = (const char *)&(urls[offset]);
				md5 = *(KeyValDB_Key *)&(urls[offset + len + 1]);
				::split(url.c_str(), host, port);
				if (url.length()) {
					urllist.push_back(std::move(url));
					urlMD5list.push_back(md5);
					hostlist.push_back(getMD5(host));
				}
				offset += len + 1 + sizeof(KeyValDB_Key);
			}
			int remain = urllist.size();
			while (remain--) {
				// find the next URL that need minimal sleep time
				int now = ::timeGetTime();
				auto getSleepTime = [&](KeyValDB_Key hostmd5) -> int {
					if (auto node = hostLastTime.find(hostmd5)) {
						if (node->m_val + DDOSDELAY - now > 0)
							return node->m_val + DDOSDELAY - now;
					}
					return 0;
				};
				int bestIndex = -1, bestSleep = 9999;
				for (int i = 0; bestSleep > 0 && i < urllist.size(); i++) {
					if (hostlist[i] == KeyValDB_Key()) continue;
					int s = getSleepTime(hostlist[i]);
					if (s < bestSleep) {
						bestSleep = s; bestIndex = i;
					}
				}
				if (bestIndex < 0) break;

				md5 = urlMD5list[bestIndex];
				url = std::move(urllist[bestIndex]);
				KeyValDB_Key hostMD5 = hostlist[bestIndex];
				g_model.m_processingUrls.insert(md5);
				if (!g_model.m_pendingURLs.remove(md5)) failedremove++;

				if (bestSleep) ::Sleep(bestSleep);
				now = ::timeGetTime();

				hostLastTime.findInsert(hostMD5, now, true);
				nextDownloadTime = now + DDOSDELAY;
				download(url, md5, host);
				processedCount++;
				hostlist[bestIndex] = KeyValDB_Key();
				if (*stop) break;
			}
		}
	}	
	g_model.m_nIdleThreads++;
}

// load a list of URL from file and push to pendingDB or push to md5->URL mapping
int loadURLList(const char *urlFile,
				bool pushPending, 
				KKHash<KeyValDB_Key, std::string> *md52URL) {
	printf("loading URL list from %s...\n", urlFile);
	g_model.m_urlFile = fopen(urlFile, "a+b");
	if (!g_model.m_urlFile) {
		printf("Failed to open %s\n", urlFile);
		return 1;
	} else {
		size_t count = 0;
		DWORD t = ::timeGetTime();
		std::vector<char> buf;
		buf.resize(1048576, 0);
		char url[MAXURLLEN * 2], c;
		int ul = 0, bufLen = 0, bufIndex = 0;
		size_t readCount = 0;
		while (bufIndex < bufLen || !feof(g_model.m_urlFile)) {
			if (bufIndex >= bufLen) {
				bufLen = fread(&buf[0], 1, buf.size(), g_model.m_urlFile);
				if (bufLen > 0) readCount += bufLen;
				bufIndex = 0;
			}
			if (!bufLen) break;
			c = buf[bufIndex++];
			if (c == '\n' || c == '\r' || ul == sizeof(url)-1) {
				url[ul] = 0;
				if (ul) {
					std::string urlstr = url;
					if (pushPending) g_model.pushPending(urlstr, false);
					if (md52URL) {
						KeyValDB_Key md5 = getMD5(urlstr);
						if (!g_model.m_contentDB ||
							(g_model.m_contentDB->exist(md5) && Model::truncateURL(urlstr))) {
							md52URL->findInsert(md5, urlstr);
						}
					}
				}
				ul = 0; count++;
				if (::timeGetTime() - t > 1000) {
					t = ::timeGetTime();
					printf("\r# loaded URL = %llu, %llu bytes read", count, readCount);
				}
			} else {
				if (ul || (c != '\r' && c != '\n'))
					url[ul++] = c;
			}
		}
	}
	printf("\n");
	g_model.print();
	return 0;
}

void monitorThread(bool *stop) {
	const int delayms = 1000;
	DWORD t1 = ::timeGetTime();
	while (!*stop) {
		DWORD t2 = ::timeGetTime();
		if (t2 >= t1 + delayms) {
			t1 = t2;
		} else {
			::Sleep(t1 + delayms - t2);
			t1 += delayms;
		}
		g_model.print();
	}
	g_model.print();
}

int runCrawling() 
{
	int nThread = 1;
	size_t maxCrawlingURL = 0;
	printf("number of threads:");
	scanf("%d", &nThread);
	if (nThread <= 0) nThread = 1;

	printf("max crawling URLs (default=%llu):", 
		   g_model.m_maxCrawlingURLs);
	scanf("%llu", &maxCrawlingURL);
	if (maxCrawlingURL > 1) g_model.m_maxCrawlingURLs = maxCrawlingURL;

	if (!g_model.initDB4Crawling(nThread)) {
		printf("Failed to init DB...\n");
		return 1;
	}

	loadURLList(URLFILE, true, 0);

	if (!g_model.m_pendingURLs.count()) {
		std::string urlSeed;
		::fflush(stdin);
		do {
			printf("\nPlease enter URL seed (e.g. http://nesdev.com/):");
			std::getline(std::cin, urlSeed);
		} while (!urlSeed.length());
		g_model.pushPending(urlSeed, true);
	}

	printf("\n");
	for (int i = 3; i > 0; i--) {
		printf("\rwait %ds to start...", i);
		::Sleep(1000);
	}

	bool stop = false, monStop = false;
	KKTask monThread(monitorThread, &monStop);
	monThread.async(THREADSTACKSIZE);

	std::vector<KKTask> threads;
	threads.resize(nThread);
	printf("\nEngine starts...\n");
	for (int i = 0; i < nThread; i++) {
		threads[i].set(crawlingThread, &stop, nThread, i);
		threads[i].async(THREADSTACKSIZE);
	}
	while (true) {
		std::string s;
		std::cin >> s;
		if (s == "stateoff") {
			g_model.m_printState = 0;
		} else if (s == "stateon") {
			g_model.m_printState = 1;
		} else if (s == "stop" || s == "exit") {
			stop = true; break;
		}
	}
	for (int i = 0; i < nThread; i++) {
		threads[i].join();
	}
	monStop = true;
	monThread.join();
	g_model.m_contentDB->flush();
	return 0;
}

int runContentShortening()
{
	if (!g_model.initDB4Crawling(1)) {
		printf("Failed to init DB...\n");
		return 1;
	}

	typedef KeyValDB<KKNoLock, KKObject> ShortenDB;
	KKLocalRef<ShortenDB> shortenDB = new ShortenDB(SHORTENDBKEY, SHORTENDBVAL, true, false);

	DWORD t = ::timeGetTime();
	size_t maxKeyIndex = g_model.m_contentDB->keyFileMaxIndex();
	size_t origBytes = 0, shortenedBytes = 0;

	printf("# pages in %s = %llu\n", (const char*)CONTENTDBVAL, maxKeyIndex);
	
	if (maxKeyIndex > g_model.m_contentDB->keyFileMaxIndex())
		maxKeyIndex = g_model.m_contentDB->keyFileMaxIndex();

	for (size_t count = 0; count < maxKeyIndex; count++) {
		KeyValDB_Key key;
		if (g_model.m_contentDB->seek(count, &key)) {
			std::vector<char> data, sData;
			g_model.m_contentDB->get(key, data);
			if (shortenData(data, sData)) {
				origBytes += data.size();
				shortenedBytes += sData.size();
				shortenDB->add(key, &(sData[0]), sData.size());
			}
		}
		if (::timeGetTime() - t >= 1000) {
			t = ::timeGetTime();
			printf("\r# shortened = %llu(%.2lf%%), origBytes = %llu, shortenedBytes = %llu, ratio = %.3lf", 
				   count, count / (double)maxKeyIndex * 100.0, 
				   origBytes, shortenedBytes, origBytes ? shortenedBytes / (double)origBytes : 0.0);
		}
	}
	return 0;
}

void getSelfURLfromContent(const std::vector<char> &content, 
						   std::string &selfURL, bool convert) {
	int p = 1;
	while (p < content.size() && p - 1 < MAXURLLEN && content[p] != '\"') p++;
	selfURL = std::string(&(content[1]), p - 1); 
	if (convert) {
		selfURL = Utils::utf8ToMultiByte(selfURL.c_str(), selfURL.length());
	}
}

void getTitlefromContent(const std::vector<char> &content, 
						 std::string &title, bool convert) {
	const static char str0[] = {"<title"}, str1[] = {"</title"};
	int offset = 0;
	int p0 = -1, p1 = -1;
	while (offset < (int)content.size()) {
		int r = Utils::findchar(&(content[offset]), (int)content.size() - offset, '<');
		if (r < 0) return;
		offset += r;
		if (offset >= (int)content.size() - 21) return;
		if (memcmp(&(content[offset]), str0, sizeof(str0)-1) == 0) {
			offset += sizeof(str0)-1; p0 = offset;
		} else if (p0 >= 0 && memcmp(&(content[offset]), str1, sizeof(str1)-1) == 0) {
			p1 = offset; break;
		}
		offset++;
	}
	if (p0 >= 0 && p1 > p0) {
		while (p0 < p1 && content[p0] != '>') p0++;
		p0++;
		while (p0 < p1 && content[p0] == ' ') p0++;
		if (p0 < p1) {
			char str[256];
			int len = 0;
			for (int i = p0; i < p1 && len < sizeof(str)-1; i++) {
				if ((unsigned char)content[i] >= ' ') str[len++] = content[i];
			}
			while (len && str[len-1] == ' ') len--;
			title = std::string(str, len);
			if (convert) title = Utils::utf8ToMultiByte(title.c_str(), title.length());
		}
	}
}

//typedef KKHash<KeyValDB_Key, RankArray> Key2Rank_;
//typedef KKHash<KeyValDB_Key, RankArray, KKNul, KKLock, KKHeapBase<sizeof(Key2Rank_::Node)> > Key2Rank;

struct LinkInfo {
	KeyValDB_Key m_from, m_to;
	int m_nLinks;
};

void rankingExtract(FILE *outFile, KKLock *flock,
					KKSemaphore *sem,
					KKAtomic<size_t> *okcount, KKAtomic<size_t> *failcount, 
					KKAtomic<size_t> *linkcount, KKAtomic<size_t> *successlink,
					KKQueue<KKTuple2<KeyValDB_Key, std::vector<char> *> > *jobs) {
	std::vector<char> wbuf;
	int wbufLen = 0;
	wbuf.resize(1048576 * 4);
	while (sem->wait(INT_MAX)) {
		if (!jobs->count()) break;

		KKTuple2<KeyValDB_Key, std::vector<char> *> job = jobs->pop()->get();
		KeyValDB_Key key = job.m_p1;
		std::vector<char> *data = job.m_p2;

		// get list of links(urls) from page content
		std::set<std::string> urls;
		std::string selfURL;

		getSelfURLfromContent(*data, selfURL); 
		if (getMD5(selfURL) != key) {
			(*failcount)++; continue;
		}
			
		std::string host;
		int port;
		split(selfURL.c_str(), host, port);
		getURLs(*data, host, port == 443, false, &urls);
		(*linkcount) += urls.size();

		//get # of ok links
		std::vector<LinkInfo> linkPairs;
		linkPairs.reserve(urls.size() + 1);
		for (std::set<std::string>::iterator it = urls.begin(); it != urls.end(); it++) {
			KeyValDB_Key key2 = getMD5(*it);
			//Key2Rank::NodeRef node2 = key2rank->find(key2);
			if (g_model.m_contentDB->exist_nolock(key2)) { 
				LinkInfo p;
				p.m_from = key, p.m_to = key2;
				linkPairs.push_back(p);
				(*successlink)++;
			}
		}
		if (linkPairs.size()) {
			for (int i = 0; i < linkPairs.size(); i++) {
				linkPairs[i].m_nLinks = linkPairs.size();
				memcpy(&(wbuf[wbufLen]), &(linkPairs[i]), sizeof(LinkInfo));
				wbufLen += sizeof(LinkInfo);
				if (wbufLen >= wbuf.size() - sizeof(LinkInfo)) {
					KKLockGuard<KKLock> g(*flock);
					::fwrite(&(wbuf[0]), sizeof(LinkInfo), wbufLen / sizeof(LinkInfo), outFile);
					wbufLen = 0;
				} else if (wbufLen >= wbuf.size() / 2) {
					bool r;
					KKLockGuard<KKLock> g(*flock, r);
					if (r) {
						::fwrite(&(wbuf[0]), sizeof(LinkInfo), wbufLen / sizeof(LinkInfo), outFile);
						wbufLen = 0;
					}
				}
			}
		}
		delete data;
		(*okcount)++;
		KKLock_Yield();
	}
	KKLockGuard<KKLock> g(*flock);
	if (wbufLen >= sizeof(LinkInfo))
		::fwrite(&(wbuf[0]), sizeof(LinkInfo), wbufLen / sizeof(LinkInfo), outFile);
}

int runRanking()
{
	const double initRank = 1.0, dampingFactor = 0.85, absMaxRank = 100.0;
	int iterations = 1;
	FILE *tmpFile = 0;

	printf("# of iterations:");
	scanf("%d", &iterations);
	if (!g_model.initDB4Ranking()) {
		printf("Failed to init DB...\n");
		return 1;
	}

	size_t maxDBIndex = g_model.m_contentDB->keyFileMaxIndex();
	printf("# downloaded pages = %llu, maxDBIndex = %llu\n", 
		   g_model.m_contentDB->count(), maxDBIndex);
	printf("Override maxDBIndex:");
	scanf("%llu", &maxDBIndex);

	printf("Max Words per Page:");
	scanf("%d", &g_model.m_maxWordPerPage);

	if (maxDBIndex <= 0 || maxDBIndex >= g_model.m_contentDB->keyFileMaxIndex()) 
		maxDBIndex = g_model.m_contentDB->keyFileMaxIndex();
	
	DWORD t0, t = ::timeGetTime();
	KKAtomic<size_t> successlink = 0;

	const int max_nthreads = 32;
	int nthreads = Utils::nCPUs();
	if (nthreads > max_nthreads) nthreads = max_nthreads;
	if (nthreads > 2) nthreads--; // save the main thread

	// initial ranking & link pre-processing
	{
		std::vector<char> wbuf;
		wbuf.resize(1048576 * 16);
		size_t count = 0, wbufLen = 0;
		for (Model::DB::K2ONodeRef node = g_model.m_contentDB->firstNode(); 
				node; node = g_model.m_contentDB->nextNode(node)) {
			node->m_val.m_tempData = RankArray(initRank);
			count++; 
		}
		printf("urlMD5 count = %lld, key2rank count = %lld\n", 
				count, g_model.m_contentDB->count());

		if (iterations <= 0) goto reverse_indexing;

		tmpFile = fopen(PENDINGRANKFILE, "rb");
		if (tmpFile) {
			::_fseeki64(tmpFile, 0, SEEK_END);
            long long s = ::_ftelli64(tmpFile);
			std::string ans;
			while (true) {
				printf("Use the existing a pending rank file(%s)?", 
					   (const char*)PENDINGRANKFILE);
				std::cin >> ans;
				if (ans == "Y" || ans == "y") {
					successlink = s / sizeof(LinkInfo);
					goto ranking_iternations;
				}
				else if (ans == "N" || ans == "n") {
					fclose(tmpFile);  tmpFile = 0;
					break;
				}
			}
		}
		tmpFile = fopen(PENDINGRANKFILE, "wb");
		KKLock flock;
		if (!tmpFile) {
			printf("failed to open %s for write\n", PENDINGRANKFILE);
			return 1;
		}

		KKAtomic<size_t> okcount = 0, failcount = 0, linkcount = 0, lastokcount = 0;
		t0 = t = ::timeGetTime();

		KKQueue<KKTuple2<KeyValDB_Key, std::vector<char> *> > jobqueue;
		KKTask helper[max_nthreads];
		KKSemaphore sem;
		for (int i = 0; i < nthreads; i++) {
			helper[i].set(&rankingExtract, tmpFile, &flock, &sem,
						  &okcount, &failcount, &linkcount, &successlink, &jobqueue);
			helper[i].async();
		}

		for (size_t i = 0; i < maxDBIndex; i++) {
			if (::timeGetTime() - t >= 1000) {
				float speed = (okcount - lastokcount) * 1000.0f / (::timeGetTime() - t);
				float avgspeed = (okcount) * 1000.0f / (::timeGetTime() - t0);
				lastokcount = okcount;
				t = ::timeGetTime();
				printf("\rLink extract:%.2lf%% #ok = %llu(cur:%.0f/s avg:%.0f/s jobqueue:%d)"
					   ", #failed:%llu, #link:%llu, #oklink:%llu ", 
					   i / (double)maxDBIndex * 100, 
					   (size_t)okcount, speed, avgspeed, 
					   (int)jobqueue.count(), (size_t)failcount, (size_t)linkcount, (size_t)successlink);
			}

			// get key(md5)
			KeyValDB_Key key;
			if (!g_model.m_contentDB->seek(i, &key)) continue;

			// get page content
			std::vector<char> *data = new std::vector<char>();
			g_model.m_contentDB->get(key, *data);
			if (!(*data).size() || (*data)[0] != '\"') {
				failcount++; continue;
			}

			// pub-sub model
			KKTuple2<KeyValDB_Key, std::vector<char> *> job;
			job.m_p1 = key;
			job.m_p2 = data;
			jobqueue.push(job);
			sem.signal(1);
			while (jobqueue.count() > 4096) { ::Sleep(1); }
		}
		sem.signal(max_nthreads);
		for (int i = 0; i < nthreads; i++) helper[i].join();
		if (wbufLen) {
			::fwrite(&(wbuf[0]), sizeof(LinkInfo), wbufLen / sizeof(LinkInfo), tmpFile);
			wbufLen = 0;
		}
		::fclose(tmpFile);
		tmpFile = 0;
	}

	// ranking iterations
ranking_iternations:
	if (!tmpFile) tmpFile = fopen(PENDINGRANKFILE, "rb");
	if (!tmpFile) {
		printf("failed to open %s for read\n", PENDINGRANKFILE);
		return 1;
	}

	printf("\n");
	t = ::timeGetTime();
	for (size_t j = 0; j < iterations; j++) {
		int nbuf = 0, nbufOffset = 0;
		LinkInfo info[4096];
		::fseek(tmpFile, 0, SEEK_SET);
		size_t failcount = 0, okcount = 0;

		for (Model::DB::K2ONodeRef node = g_model.m_contentDB->firstNode(); 
				node; node = g_model.m_contentDB->nextNode(node)) {
			node->m_val.m_tempData.rank[j&1] = 0; // cleanup
		}
		for (size_t i = 0; i < successlink; i++, nbufOffset++) {
			if (i == 0 || i == successlink - 1 || ::timeGetTime() - t >= 1000) {
				t = ::timeGetTime();
				printf("\rRanking iter %d: %.2lf%% processed %llu total %llu ok %llu failed %llu ", 
					   j, i / (double)successlink * 100, i, successlink, okcount, failcount);
			}
			if (nbufOffset >= nbuf) {
				nbuf = ::fread(&(info[0]), sizeof(info[0]), sizeof(info)/sizeof(info[0]), tmpFile);
				if (!nbuf) {
					printf("failed to read linkInfo file...\n"); break;
				}
				nbufOffset = 0;
			}
			KeyValDB_Key key = info[nbufOffset].m_from, key2 = info[nbufOffset].m_to;
			int currentoklink = info[nbufOffset].m_nLinks;

			// current ranking
			double currank = 0, nextrank;
			//Key2Rank::NodeRef k2rnode = key2rank.find(key);

			RankArray *rankArray = g_model.m_contentDB->getTempData(key);
			if (!rankArray) {
				failcount++; continue;
			} else {
				currank = rankArray->rank[(j+1)&1];
				nextrank = currank * dampingFactor + (1.0 - dampingFactor);
			}
			//if (!k2rnode) {
			//	failcount++; continue; // rank not exist ????
			//} else {
			//	currank = k2rnode->m_val.rank[(j+1)&1];
			//	nextrank = currank * dampingFactor + (1.0 - dampingFactor);
			//}

			// add rank
			if (currentoklink && nextrank) {
				double drank = nextrank / currentoklink;
				rankArray = g_model.m_contentDB->getTempData(key2);
				if (rankArray) {
					rankArray->rank[j&1] += drank; okcount++;
				} else failcount++;
			}
		}
		printf("\n");
	}
	::fclose(tmpFile);

	// refine final rank
	{
		float maxRank = 0.0, minRank = 0.0, sumRank = 0.0;
		for (Model::DB::K2ONodeRef node = g_model.m_contentDB->firstNode(); 
				node; node = g_model.m_contentDB->nextNode(node)) {
			float rank = node->m_val.m_tempData.rank[(iterations-1)&1];
			rank = rank * dampingFactor + (1.0 - dampingFactor);
			if (rank > absMaxRank) rank = absMaxRank;
			node->m_val.m_tempData.rank[(iterations-1)&1] = rank;
			if (!minRank || rank < minRank) minRank = rank;
			if (!maxRank || rank > maxRank) maxRank = rank;
			sumRank += rank;
		}
		printf("maxRank %.6f minRank %.6f avgRank %.6f\n", maxRank, minRank, sumRank / g_model.m_contentDB->count());
	}
	
reverse_indexing:
	printf("Ranking finished. Reverse Indexing starts...\n");
	
	GetWords_CommonParas params;

	typedef ExternalSorter<DictWord, KKObject> ExSorter;
	params.m_finalDict = new ExSorter(FINALDICT, true, -1, 2048 * 1048576ull / sizeof(DictWord), 10);

	typedef ExternalSorter<DictWordSmall, KKObject> ExSorterSmall;
	params.m_finalDictSmall = 
		new ExSorterSmall(FINALDICTSMALL, true, -1, 640 * 1048576ull / sizeof(DictWordSmall), 4);

	typedef KKQueue<KKLocalRef<GetWordsParam> > Queue;
	Queue getWordsInQueue, getWordsOutQueue;
	KKSemaphore getWordsSem;
	KKTask tasks[max_nthreads];
	for (int k = 0; k < nthreads; k++) {
		tasks[k].set(getWords_helper, &getWordsInQueue, &getWordsOutQueue, &getWordsSem, &params);
		tasks[k].async();
	}
	
	t0 = t = ::timeGetTime();
	for (size_t i = 0; i < maxDBIndex;) {
		if (::timeGetTime() - t >= 1000 || i == maxDBIndex - 1) {
			t = ::timeGetTime();
			printf("\r#pages %llu(%.2lf%% %d/s) words:%llu non-En-Words:%llu maxWords:%llu"
				   " smallWords:%llu inQueue:%d outQueue:%d  ", 
				   (size_t)params.m_getWordFinishCount, 
				   (size_t)params.m_getWordFinishCount / (double)maxDBIndex * 100, 
				   (size_t)params.m_getWordFinishCount * 1000 / (t - t0), 
				   (size_t)params.m_wordCount, 
				   (size_t)params.m_unicodeWordCount, 
				   (size_t)params.m_maxWordOfPage,
				   (size_t)params.m_smallWordCount,
				   (int)getWordsInQueue.count(), (int)getWordsOutQueue.count());
		}
		int inCount = getWordsInQueue.count(), outCount = getWordsOutQueue.count();
		if (inCount + outCount < 4096) {
			for (int j = 0; j < (inCount < 2048 ? 2 : 1); j++) {
				KKLocalRef<GetWordsParam> job = new GetWordsParam();
				KeyValDB_Key pageMD5;
				if (i < maxDBIndex && g_model.m_contentDB->seek(i, &(pageMD5))) {
					job->m_pageIndex = i;
					//Key2Rank::NodeRef rnode = key2rank.find(pageMD5);
					RankArray *ra = g_model.m_contentDB->getTempData(pageMD5); 
					job->m_pageRank = ra ? ra->rank[(iterations-1)&1] : initRank;
					g_model.m_contentDB->get(pageMD5, job->m_pageData);
					getWordsInQueue.push(job);
					getWordsSem.signal(1);
					i++;
				} else i = maxDBIndex;
			}
		} else if (outCount < 1024) {
			if (outCount < 100) ::Sleep(1);
			else KKLock_Yield();
		}
	}
	getWordsSem.signal(0x3fffffff);
	for (int k = 0; k < nthreads; k++) tasks[k].join();

	printf("\nWord/Pharse extraction completed, closing database...\n");
	g_model.m_contentDB = 0; // release database.
	printf("Reverse Indexing, merge sort begins...\n");
	t = ::timeGetTime();
	size_t freeRAM = Utils::nFreeRAM() * 0.75;
	if (freeRAM < 128 * 1048576) freeRAM = 128 * 1048576;
	params.m_finalDict->exSort(true, freeRAM / sizeof(DictWord)); // 4G RAM
	printf("main word/phrase dictionary built.\n");
	freeRAM = Utils::nFreeRAM() * 0.5;
	if (freeRAM < 128 * 1048576) freeRAM = 128 * 1048576;
	params.m_finalDictSmall->exSort(true, freeRAM / sizeof(DictWordSmall)); // 2G RAM
	printf("small word dictionary built.\n");
	t = ::timeGetTime() - t;
	printf("\nReverse Indexing finished!!! time:%ldms\n", (int)t);
	return 0;
}

void testDownload() {
	while (true) {
		std::string url, host;
		int port = 0, delay = 10, tryCount = 1;

		printf("Please input URL:");
		std::cin >> url;
		printf("Please input tryCount:");
		scanf("%d", &tryCount);
		printf("Please input delay:");
		scanf("%d", &delay);

		const char *path = split(url.c_str(), host, port);
		std::vector<char> data;

		int fileindex = 0;
		while (tryCount--) {
			data.clear();
			bool r = downloadEx(host.c_str(), port, path, data);
			if (r && data.size()) {
				printf("%d bytes downloaded...", data.size());
				char fname[2100];
				for (int i = 0; i < host.length(); i++) {
					fname[i] = isValidWordChar(host[i]) ? host[i] : '_';
				}
				sprintf(&fname[host.length()], "_%d.html", fileindex);
				FILE *fout = fopen(fname, "wb");
				if (fout) {
					fwrite(&(data[0]), 1, data.size(), fout);
					fclose(fout);
					printf("saved to %s\n", fname);
				} else {
					printf("error saving\n");
				}
			}
			fileindex++;
			Sleep(delay * 1000);
		}
	}
}

extern int searchPages(); // Searcher.cpp

int _tmain(int argc, _TCHAR* argv[])
{
	KKSocket_Startup();

	int sel = 0;
	std::cout << "Last build time:" << __DATE__ << " " << __TIME__ << "\n";
	int maxstdio = ::_setmaxstdio(2048);
	std::cout << "Max stdio count is " << maxstdio << std::endl;
	printf("1. run URL crawling\n");
	printf("2. run content shortening (deprecated)\n");
	printf("3. start ranking downloaded content\n");
	printf("4. search pages\n");
	printf("101. exteral sort test\n");
	printf("102. winHttp page download\n");
	printf("103. database compress test\n");
	printf("Please select:");
	scanf("%d", &sel);
	if (sel == 1) return runCrawling();
	else if (sel == 2) return runContentShortening();
	else if (sel == 3) return runRanking();
	else if (sel == 4) return searchPages();
	else if (sel == 101) {
		int nThreads = 2;
		printf("number of threads:");
		scanf("%d", &nThreads);
		DWORD t = ::timeGetTime();
		externalSortTest(nThreads);
		t = ::timeGetTime() - t;
		printf("time:%dms\n", t);
		return 0;
	}
	else if (sel == 102) {
		testDownload();
	}
	else if (sel == 103) {
again:
		std::string filename;
		std::vector<char> raw;
		printf("input file:");
		std::cin >> filename;
		FILE *f = ::fopen(filename.c_str(), "rb");
		if (!f) {
			printf("failed to open\n"); goto again;
		}
		while (!feof(f)) {
			char c = ::fgetc(f); 
			if (feof(f)) break;
			raw.push_back(c);
		}
		int len = raw.size();
		int compSize = KeyValDB_compressTest(len ? &(raw[0]) : 0, len, (filename + ".out").c_str());
		if (compSize < 0) printf("compress failed:%d\n", compSize);
		else printf("compress OK, orig %d compress %d (%.3lf%%)\n", 
			        len, compSize, compSize / (double)len * 100.0);
		goto again;
	}
	else return 1;
}

