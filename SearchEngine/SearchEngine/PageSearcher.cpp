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
#include "BufferReader.h"

struct SearchParams {
	bool  m_mergeHost;
	int   m_maxShow;
	int   m_maxSearch;

	std::vector<KeyValDB_Key> m_pageMD5list;

	SearchParams() : m_mergeHost(true), m_maxShow(200), m_maxSearch(100000000) { }
};


template <typename DictWordT>
void searchRange(FILE *dictFile,
				 const std::string &inword, size_t *startIndex, size_t *endIndex) 
{
	::_fseeki64(dictFile, 0, SEEK_END);
	long long fs = ::_ftelli64(dictFile);
	size_t nWords = fs > 0 ? fs / sizeof(DictWordT) : 0;
	size_t lo = 0, hi = nWords;
	while (lo < hi - 1) {
		size_t mid = (lo + hi)/2;
		::_fseeki64(dictFile, mid * sizeof(DictWordT), SEEK_SET);
		DictWordT word;
		fread(&word, sizeof(DictWordT), 1, dictFile);
		int r = word.cmp(inword);
		if (r >= 0) hi = mid;
		else lo = mid;
	}
	*startIndex = lo + 1; hi = nWords;
	while (lo < hi - 1) {
		size_t mid = (lo + hi)/2;
		::_fseeki64(dictFile, mid * sizeof(DictWordT), SEEK_SET);
		DictWordT word;
		fread(&word, sizeof(DictWordT), 1, dictFile);
		int r = word.cmp(inword);
		if (r > 0) hi = mid;
		else lo = mid;
	}
	*endIndex = hi;
}

void splitSentence(const std::string &s, std::vector<std::string> &words,
				   std::string &outSentence) 
{
	std::vector<char> word;
	word.resize(s.length() + 1, 0);
	outSentence = "";
	int wl = 0;
	for (int i = 0; i < s.length(); i++) {
		if (s[i] == '\"') {
			if (wl) {
				word[wl] = 0; words.push_back(&(word[0]));	
				if (outSentence.length()) outSentence += " ";
				outSentence += std::string(&(word[0]));
			}
			wl = 0;
			i++;
			while (i < s.length() && s[i] != '\"') word[wl++] = s[i++];
			if (wl) {
				word[wl] = 0; words.push_back(&(word[0]));	
				if (outSentence.length()) outSentence += " ";
				outSentence += std::string(&(word[0]));
			}
			wl = 0;
			i++;
		}
		if ((unsigned char)s[i] <= ' ') { // delimeter
			if (wl) {
				word[wl] = 0; words.push_back(&(word[0]));
				if (outSentence.length()) outSentence += " ";
				outSentence += std::string(&(word[0]));
			}
			wl = 0;
		} else word[wl++] = s[i];
	}
	if (wl) {
		word[wl] = 0; words.push_back(&(word[0]));	
		if (outSentence.length()) outSentence += " ";
		outSentence += std::string(&(word[0]));
	}
}


void cmdExec(std::vector<std::string> &words, SearchParams &params) {
	if (words[0] == "#help") {
		printf("#help: display this help\n");
		printf("#get index [outfile]: display/export page content of index\n");
		printf("#merge: merge same hosts\n");
		printf("#unmerge: not merge same hosts\n");
		printf("#maxSearch: value\n");
		printf("#maxShow: value\n");
	} else if (words[0] == "#get") {
		uint32_t index = 0;
		FILE *f = 0;
		if (words.size() < 2 || words[1].length() <= 0 ||
			sscanf(words[1].c_str(), "%u", &index) != 1) {
			printf("invalid index\n"); return;
		}
		if (words.size() >= 3) {
			f = ::fopen(words[2].c_str(), "wb");
		}
		KeyValDB_Key pageMD5;
		if (!g_model.m_contentDB->seek(index, &pageMD5)) {
			printf("index out of bound\n"); return;
		}
		std::vector<char> pageContent;
		g_model.m_contentDB->get(pageMD5, pageContent);
		if (pageContent.size()) 
			::fwrite(&(pageContent[0]), 1, pageContent.size(), f ? f : stdout);
		printf("\nOK: %d bytes", pageContent.size());
		if (f) ::fclose(f);
	} else if (words[0] == "#merge") {
		params.m_mergeHost = true;
	} else if (words[0] == "#unmerge") {
		params.m_mergeHost = false;
	} else if (words[0] == "#maxShow" || words[0] == "#maxSearch") {
		int v;
		if (words.size() >= 2 && sscanf(words[1].c_str(), "%d", &v) == 1) {
			if (words[0] == "#maxShow") params.m_maxShow = v;
			else params.m_maxSearch = v;
		}
	}
}

namespace {
#pragma pack(push, 1)
	struct PosRanks 
	{
		enum _ { Max = 2 };
		int8_t   m_count;
		float    m_rank[Max];
		uint16_t m_pos[Max];
		PosRanks() { memset(this, 0, sizeof(*this)); }
		inline void add(float rank, uint16_t pos) {
			if (m_count <= Max - 1) {
				m_pos[m_count] = pos; m_rank[m_count] = rank;
				m_count++;
				return;
			}
			int minRankInd = 0;
			for (int i = 1; i < Max; i++) {
				if (m_rank[i] < m_rank[minRankInd]) minRankInd = i;
			}
			if (m_rank[minRankInd] < rank) {
				m_rank[minRankInd] = rank; m_pos[minRankInd] = pos;
			}
		}
		inline float totalRank() {
			float sum = 0;
			for (int i = 0; i < Max; i++) sum += m_rank[i];
			return sum;
		}
		inline int positionDiff(const PosRanks &other) {
			int minPosDiff = 65535;
			for (int i = 0; i < m_count; i++) {
				for (int j = 0; j < other.m_count; j++) {
					int diff = (int)m_pos[i] - (int)other.m_pos[j];
					if (diff < 0) diff = -diff;
					if (diff < minPosDiff) minPosDiff = diff;
				}
			}
			return minPosDiff;
		}
		std::string toString() {
			char str[Max * 8 + 16];
			int len = 0;
			for (int i = 0; i < m_count; i++) {
				if (!m_pos[i]) break;
				len += sprintf(str + len, "%s%d", (i ? "-":""), m_pos[i]);
			}
			len += sprintf(str+len, "(r:%.2e)", totalRank());
			return std::string(str);
		}
	};
	struct WordItem { // word items in memory
		uint32_t  m_pageInd;
		uint16_t  m_position;
		float     m_rank;
	};
	KKAssert(sizeof(WordItem)==10);
#pragma pack(pop)
	struct Range { // range for each token
		WordItem  *m_wordItem;
		size_t    m_wordItemCount;
		Range() { memset(this, 0, sizeof(*this)); }
	};
	int hashInitBit(size_t v) {
		int r = 2;
		while ((1ull << r) < v) r++;
		return r;
	}

	// Page2WordRanks: page-> ([word0,rank0] [word1,rank1],...)
	const int MaxTerms = 8;
	typedef KKHash<uint32_t, KKFixedArray<PosRanks, MaxTerms>, 
		           KKObject, KKNoLock, KKNul, KKHashFn<uint32_t>, true> Page2WordRanks;

	// Rank2Page: resulting rank->page
	typedef KKAVLTree<float, uint32_t, KKNul, KKNoLock> Rank2Page;
}

void joinThread(SearchParams *params, Range *ranges, 
				KKLocalRef<Page2WordRanks> *page2Rank_,
				std::vector<Page2WordRanks::Node> *page2RankBuf,
				Rank2Page *rank2Page,
		        int threadInd, uint32_t threadMask, size_t *nResults)
{
	int nTerms = 1;
	*nResults = 0;
	(*page2Rank_) = 
	  new Page2WordRanks(hashInitBit(ranges[0].m_wordItemCount / (threadMask+1)), 2, 
		                 &((*page2RankBuf)[0]), (*page2RankBuf).size());
	Page2WordRanks *page2Rank = (*page2Rank_).get();
	for (int k = 1; k < MaxTerms; k++) {
		if (ranges[k].m_wordItem) nTerms = k + 1;
	}
	// first search term
	for (int w = 0; w < ranges[0].m_wordItemCount; w++) {
		WordItem *word = &(ranges[0].m_wordItem[w]);
		if ((word->m_pageInd & threadMask) != threadInd) continue;
		Page2WordRanks::NodeRef node = page2Rank->find(word->m_pageInd);
		if (node) {
			node->m_val[0].add(word->m_rank, word->m_position);
		} else {
			KKFixedArray<PosRanks, MaxTerms> v(nTerms);
			v[0].add(word->m_rank, word->m_position);
			page2Rank->insert(word->m_pageInd, v);
		}
	}
	// other search terms
	for (int k = 1; k < nTerms; k++) {
		for (int w = 0; w < ranges[k].m_wordItemCount; w++) {
			WordItem *word = &(ranges[k].m_wordItem[w]);
			if ((word->m_pageInd & threadMask) != threadInd) continue;
			Page2WordRanks::NodeRef node = page2Rank->find(word->m_pageInd);
			if (node) {
				node->m_val[k].add(word->m_rank, word->m_position);
			}
		}
	}

	float minRank = 0;
	// build up page->rank mapping
	for (Page2WordRanks::NodeRef node = page2Rank->first();
		node; node = page2Rank->next(node)) {
		float sumRank = 0.0, coRank = 1.0;
		for (int i = 0; i < nTerms; i++) {
			float termRank = node->m_val[i].totalRank();
			if (termRank <= 0.0f) goto l__next;
			sumRank += termRank;
			if (i > 0) {
				int posDiff = node->m_val[i-1].positionDiff(node->m_val[i]);
				float t = powf(0.9f, posDiff - 1);
				if (t < 1e-6) t = 1e-6;
				coRank *= t;
			}
		}
		(*nResults)++;
		uint32_t pageInd = node->m_key;
		float finalRank = sumRank * coRank;
		if (finalRank <= minRank) continue;
		rank2Page->insert(finalRank, pageInd);
		if (rank2Page->count() > params->m_maxShow) {
			Rank2Page::NodeRef node2 = rank2Page->first();
			Rank2Page::NodeRef node3 = node2 ? rank2Page->next(node2) : 0;
			if (node3) minRank = node3->key();
			if (node2) rank2Page->remove(node2);
		}
l__next:
		;
	}
}

bool buildContentMD5(SearchParams *param) {
	size_t dbsize = g_model.m_contentDB->count();
	if (!dbsize) return false;
	param->m_pageMD5list.resize(dbsize);
	FILE *file = fopen(CONTENTMD5, "rb");
	if (file) {
		::_fseeki64(file, 0, SEEK_END);
		long long s = ::_ftelli64(file);
		size_t ok = 0;
		if (s >= dbsize * 16) {
			::_fseeki64(file, 0, SEEK_SET);
			KeyValDB_Key hostmd5;
			while (::fread(&hostmd5, sizeof(hostmd5), 1, file) > 0) {
				param->m_pageMD5list[ok++] = hostmd5;
			}
		}
		::fclose(file);
		if (ok == dbsize) {
			printf("Successfully read MD5 for %llu pages\n", dbsize);
			return true;
		}
	}
	printf("Building MD5 for each downloaded page...\n");
	file = fopen(CONTENTMD5, "wb");
	if (!file) {
		printf("Failed to open %s for write...\n", (const char*)CONTENTMD5);
		return false;
	}
	for (size_t i = 0; i < dbsize; i++) {
		std::vector<char> data;
		KeyValDB_Key key;
		if (!g_model.m_contentDB->seek(i, &key)) continue;
		g_model.m_contentDB->get(key, data);
		std::string url, host, title;
		int port;
		getSelfURLfromContent(data, url, true);
		::split(url.c_str(), host, port);
		KeyValDB_Key hostmd5 = getMD5(host);
		param->m_pageMD5list[i] = hostmd5;
		//::fwrite(&hostmd5, sizeof(hostmd5), 1, file);
	}
	::fwrite(&(param->m_pageMD5list[0]), sizeof(KeyValDB_Key), param->m_pageMD5list.size(), file);
	fclose(file);
	printf("Successfully build MD5 for %llu pages\n", dbsize);
	return true;
}

int searchPages() 
{
	SearchParams params;

	if (!g_model.initDB4Searching()) {
		printf("Failed to init DB...\n");
		return 1;
	}

	FILE *dictFile = fopen(FINALDICT, "rb");
	FILE *dictFileSmall = fopen(FINALDICTSMALL, "rb");
	if (!dictFile) {
		printf("Failed to open %s, please run the previous process\n", FINALDICT);
		return 1;
	}
	::_fseeki64(dictFile, 0, SEEK_END);
	long long fs = _ftelli64(dictFile);
	size_t nWords = fs > 0 ? fs / sizeof(DictWord) : 0;
	printf("DBSize:%llu, nWords:%llu, WordPerPages:%llu\n", 
		   g_model.m_contentDB->count(), nWords,
		   g_model.m_contentDB->count() ? nWords / g_model.m_contentDB->count() : 0);
	if (!nWords) return 2;

	buildContentMD5(&params);
	g_model.m_contentDB->createReadBuf(1024, 4096);

	const int maxThreads = 32;
	int nJointThreads = Utils::nCPUs();
	if (nJointThreads > maxThreads) nJointThreads = maxThreads;
	std::vector<Page2WordRanks::Node> page2WordRankBuf[maxThreads];
	for (int i = 0; i < nJointThreads; i++)
	  page2WordRankBuf[i].resize((g_model.m_contentDB->count() / nJointThreads) + 2);

	std::vector<WordItem> wordItemBuf;
	wordItemBuf.resize((g_model.m_contentDB->count() + 1) * MaxTerms / 2);

	while (true) {

		//KKLocalRef<Page2WordRanks> pages;
		KKBufferReader dictFileReader(dictFile), dictFileSmallReader(dictFileSmall);
		Range ranges[MaxTerms];

		// wait for input
		std::string s_, inSentence, sentence;
		printf("\nPlease input search item:");
		std::getline(std::cin, s_);
		inSentence = Utils::multiByteToUTF8(s_.c_str(), s_.length());
		std::vector<std::string> words;
		std::vector<size_t> resultCount;

		splitSentence(inSentence, words, sentence);
		if (!words.size()) continue;

		if (words[0].length() && words[0][0] == '#') {
			cmdExec(words, params); 
			continue;
		}

		DWORD t0 = ::timeGetTime();

		size_t wordItemCount = 0;
		for (int k = 0; k < words.size() && k < MaxTerms; k++) {
			size_t startIndex = 0, endIndex = 0;
			FILE *targetDict = 0;
			if (dictFileSmall && words[k].length() <= DictWordSmall::MaxWordLen) {
				targetDict = dictFileSmall;
				searchRange<DictWordSmall>(targetDict, words[k], &startIndex, &endIndex);
			}
			if (endIndex <= startIndex) {
				targetDict = dictFile;
				searchRange<DictWord>(targetDict, words[k], &startIndex, &endIndex);
			}
			printf("%s: %llu-%llu(%llu)\n", 
				   (targetDict == dictFileSmall ? "smallDict" : "mainDict"), 
				   startIndex, endIndex, endIndex-startIndex);
			resultCount.push_back(endIndex - startIndex);

			if (endIndex - startIndex > params.m_maxSearch) startIndex = endIndex - params.m_maxSearch;
			if (endIndex > nWords) endIndex = nWords;

			ranges[k].m_wordItem = &(wordItemBuf[wordItemCount]);
			size_t oldCount = wordItemCount;
			for (size_t i = startIndex; i < endIndex && wordItemCount < wordItemBuf.size(); i++) {
				WordItem *worditem = &(wordItemBuf[wordItemCount]);
				if (targetDict == dictFile) {
					DictWord word_(DictWord::NoInit);
					if (dictFileReader.seekRead(i * sizeof(DictWord), 
					                        (char *)&word_, sizeof(DictWord), 1) != 1) break;
					worditem->m_pageInd = word_.m_pageIndex;
					worditem->m_position = word_.m_position;
					worditem->m_rank = word_.m_rank;
					wordItemCount++;
				} else if (targetDict == dictFileSmall) {
					DictWordSmall wordM_(DictWordSmall::NoInit);
					if (dictFileSmallReader.seekRead(i * sizeof(DictWordSmall), 
					                        (char *)&wordM_, sizeof(DictWordSmall), 1) != 1) break;
					worditem->m_pageInd = wordM_.m_pageIndex;
					worditem->m_position = wordM_.m_position;
					worditem->m_rank = wordM_.m_rank;
					wordItemCount++;
				}
			}	
			ranges[k].m_wordItemCount = wordItemCount - oldCount;
		}

		// join thread
		DWORD t1 = ::timeGetTime();

		KKLocalRef<Page2WordRanks> page2WRanks[maxThreads];
		Rank2Page rank2Page_[maxThreads], rank2Page;
		KKTask joinTask[maxThreads];
		size_t nResults_[maxThreads], nResults = 0;
		for (int i = 0; i < nJointThreads; i++) {
			joinTask[i].set(&joinThread, &params, ranges, 
				            &(page2WRanks[i]), &(page2WordRankBuf[i]), &(rank2Page_[i]), i, 
							(uint32_t)(nJointThreads-1), &(nResults_[i]));
			joinTask[i].async();
		}
		for (int i = 0; i < nJointThreads; i++) {
			joinTask[i].join();
			nResults += nResults_[i];
			for (Rank2Page::NodeRef node = rank2Page_[i].first(); 
				 node; node = rank2Page_[i].next(node)) {
				rank2Page.insert(node->m_key, node->m_val);
			}
		}

		DWORD t2 = ::timeGetTime();

		// start to marshall
		{
			std::vector<std::string> outLines;
			std::vector<std::string> outHosts;
			std::map<KeyValDB_Key, int> seenHosts;
			Rank2Page::NodeRef node = rank2Page.last();
			int showCount = params.m_maxShow;
			for (; node && (showCount--); node = rank2Page.prev(node)) {
				std::vector<char> pageContent;
				
				KeyValDB_Key hostmd5 = params.m_pageMD5list[node->m_val];
				if (params.m_mergeHost && seenHosts.find(hostmd5) != seenHosts.end()) {
					seenHosts[hostmd5]++;
					showCount++;
					continue;
				}

				KeyValDB_Key pageMD5;
				if (g_model.m_contentDB->seek(node->m_val, &pageMD5))
					g_model.m_contentDB->get(pageMD5, pageContent);
				if (!pageContent.size()) continue;

				std::string url, host, title;
				int port;
				getSelfURLfromContent(pageContent, url, true); 
				::split(url.c_str(), host, port);

				seenHosts[hostmd5] = 1;
				Page2WordRanks::NodeRef wordNode = 
					page2WRanks[node->m_val & (nJointThreads-1)]->find(node->m_val);
				std::string posStr;
				if (wordNode) {
					posStr += "[";
					for (int i = 0; i < wordNode->m_val.size(); i++) {
						posStr += wordNode->m_val[i].toString();
						if (i) {
							char s[16];
							sprintf(s, "(c:%d)", wordNode->m_val[i-1].positionDiff(wordNode->m_val[i]));
							posStr += s;
						}
						posStr += " ";
					}
					posStr += "]";
				}
				//if (wordNode) {
				//	displayWord = Utils::utf8ToMultiByte((const char *)(wordNode->m_val.m_word), 
				//										 (int)wordNode->m_val.wordLen());
				//}
				getTitlefromContent(pageContent, title, true);
				char line[8192];
				int len = 0;
				len = sprintf(line, "%u:%.3e %s [%s] [%s]\n", node->m_val,
				 	          node->m_key, posStr.c_str(), title.c_str(), url.c_str());
				outLines.push_back(line);
				outHosts.push_back(host);
			}
			for (int i = outLines.size() - 1; i >= 0; i--) {
				::fwrite(outLines[i].c_str(), 1, outLines[i].length(), stdout);
				int dup;
				if ((dup = seenHosts[getMD5(outHosts[i])]) > 1) {
					printf("(+%d pages for %s)\n", (int)dup - 1, outHosts[i].c_str());
				}
			}
		}

		for (int i = 0; i < words.size(); i++) {
			std::string displayWord = Utils::utf8ToMultiByte(words[i].c_str(), words[i].length());
			printf("[%s]:%llu ", (const char *)displayWord.c_str(), resultCount[i]);
		}
		//if (words.size() > 1) {
		//	std::string displaySentence = Utils::utf8ToMultiByte(sentence.c_str(), sentence.length());
		//	printf("[%s]:%d ", displaySentence.c_str(), sentenceResultCount);
		//}
		printf("utf8:");
		for (int i = 0; i < sentence.length(); i++) {
			printf("%02x ", (int)(unsigned char)sentence[i]);
		}

		DWORD t3 = ::timeGetTime();
		printf("\nFinal:%llu, read time:%dms, join/sort time:%dms, marshall time:%dms\n", 
			   nResults, (int)(t1-t0), (int)(t2-t1), (int)(t3-t2));
	}
}
