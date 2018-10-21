
#include "stdafx.h"
#include <vector>

namespace KeyValDB {
	// tableSize need to be power of 2
	static const int tableSize = 65536 * 4, collision = 3;
	static const int bit1 = 2;
	static const int bit2 = 5;

	inline unsigned int hash4(const unsigned char *p) {
		unsigned int v = ((*(unsigned int*)p) & 0xffffff) * 0x9e3779b9;
		v = v ^ (v >> 16);
		return v;
	}
	inline unsigned char translate(unsigned char c) {
		if (c >= 'a' && c <= 'z') {
			return c - 'a' + '9' + 1;
		} else if (c >= 'A' && c <= 'Z') {
			return c - 'A' + '9' + 27;
		} else if (c > '9' && c < 'A') {
			return c + 52;
		} else if (c > 'Z' && c < 'a') {
			return c + 26;
		} else return c;
	}
	inline unsigned char untranslate(unsigned char c) {
		if (c > '9' && c <= '9' + 26) {
			return 'a' + c - '9' - 1;
		} else if (c > '9' + 26 && c <= '9' + 52) {
			return 'A' + c - '9' - 27;
		} else if (c > '9' + 52 && c <= '9' + 52 + 7) {
			return c - '9' - 52 + '9';
		} else if (c > '9' + 52 + 7 && c <= '9' + 52 + 13) {
			return c - '9' - 52 - 7 + 'Z';
		} else return c;
	}
}

void *KeyValDB_compress_alloc() {
	return ::malloc(KeyValDB::tableSize * sizeof(int));
}
void KeyValDB_compress_free(void *p) {
	::free(p);
}

int KeyValDB_compress0(const unsigned char *in, int len, 
					   std::vector<unsigned char> *out, void *memory) {
	using namespace KeyValDB;

	int *table = (int *)memory;
	if (!memory) table = (int *)KeyValDB_compress_alloc();

	for (int i = 0; i < tableSize; i++) table[i] = -1;

	size_t osize = out->size();
	int i = 0, written = 0;
	for (;i < len; i++) {
		if (i >= len - 4) {
			if (in[i] >= 0xfc) {
				out->push_back(0xfc);
				out->push_back(in[i]);
			} else out->push_back(in[i]);
			continue;
		}
		unsigned int v = hash4(in + i);
		unsigned ti = v % tableSize;
		int okindex = -1, maxrepeat = 0, maxsave = 0;
		for (int k = 0; k < collision; k++) {
			unsigned tii = (ti + k) % tableSize;
			if (table[tii] < 0) break;
			if (table[tii] <= i - 65535) continue;
			int repeat = 0;
			int i0 = table[tii];
			while (i + repeat < len - 8 && repeat < 255-8 &&
				   *(unsigned long long *)&(in[i0 + repeat]) == 
				   *(unsigned long long *)&(in[i + repeat]))
				   repeat += 8; // turbo
			if (repeat < maxrepeat - 7) continue;
			while (i + repeat < len && repeat-3 < 255 &&
		           in[i0 + repeat] == in[i + repeat]) repeat++;
			int di = i - i0;
			int nsave = di < (1<<(8-bit1)) && repeat-3 < (1<<bit1) ? repeat - 2 :
				        di < (1<<(16-bit2)) && repeat-3 < (1<<bit2) ? repeat - 3 : repeat - 4;
			if (nsave > maxsave) {
				maxsave = nsave, maxrepeat = repeat, okindex = i0;
			}
		}
		if (!maxsave) {
			if (in[i] >= 0xfc) {
				out->push_back(0xfc);
				out->push_back(in[i]);
			} else out->push_back(in[i]);
		} else if (maxsave == maxrepeat - 2) {
			int t = ((i - okindex) << bit1) + maxrepeat-3; 
			out->push_back(0xfd);
			out->push_back(t);
		} else if (maxsave == maxrepeat - 3) {
			int t = ((i - okindex) << bit2) + maxrepeat-3; 
			out->push_back(0xfe);
			out->push_back(t);
			out->push_back(t>>8);
		} else {
			out->push_back(0xff);
			out->push_back((i - okindex) & 0xff);
			out->push_back(((i - okindex) >> 8));
			out->push_back(maxrepeat-3);
		}
		unsigned minti = ti;
		for (int k = 0; k < collision; k++) {
			unsigned tii = (ti + k) % tableSize;
			if (table[tii] < 0 || table[tii] <= i - 65535) {
				table[tii] = i; minti = INT_MAX; break;
			}
			if (table[tii] < table[minti]) minti = tii;
		}
		if (minti != INT_MAX) table[minti] = i;
		if (maxrepeat) {
			for (int j = 1; j < maxrepeat && i + j < len - 4; j++) {
				v = hash4(in + (i + j));
				minti = v % tableSize;
				int k;
				for (k = 0; k < collision; k++) {
					ti = (v + k) % tableSize;
					if (table[ti] < 0 || table[ti] < i + maxrepeat - 65535) {
						table[ti] = i + j; break;
					}
					if (table[ti] < table[minti]) minti = ti;
				}
				if (k >= collision) table[minti] = i + j;
			}
			i += maxrepeat - 1;
		}
	}

	if (!memory) KeyValDB_compress_free(table);
	return out->size() - osize;
}

int KeyValDB_decompress0(unsigned char *in, int len, std::vector<char> *decode) 
{
	using namespace KeyValDB;
	size_t osize = decode->size();
	for (int i = 0; i < len;) {
		unsigned char c = in[i++];
		if (c <= 0xfc) {
			if (c == 0xfc) {
				if (i >= len) break;
				decode->push_back(in[i++]);
			}
			else decode->push_back(c);
			continue;
		}
		int di, rpt, len2;
		if (c == 0xff) {
			if (i + 3 > len) break;
			di = *(unsigned short *)&(in[i]);
			i+=2;
			rpt = (int)in[i++] + 3;
		} else if (c == 0xfe) {
			if (i + 2 > len) break;
			int t = *(unsigned short *)&(in[i]);
			i+=2;
			di = (t>>bit2);
			rpt = (t&((1<<bit2)-1))+3;
		} else {
			if (i+1 > len) break;
			int t = *(unsigned char *)&(in[i]);
			i+=1;
			di = (t>>bit1);
			rpt = (t&((1<<bit1)-1))+3;
		}
		len2 = decode->size();
		if (di > len2) {
			break; // failed!!!
		}
		decode->resize(len2 + rpt);
		int j = 0;
		if (di >= 8) {
			while (j < rpt - 7) {
				*(unsigned long long *)&((*decode)[len2 + j]) 
					= *(unsigned long long *)&((*decode)[len2 - di + j]);
				j += 8;
			}
		}
		for (; j < rpt; j++) {
			(*decode)[len2 + j] = (*decode)[len2 - di + j];
		}
	}
	return decode->size() - osize;
}

int KeyValDB_compress1(const unsigned char *in, int len, 
					   std::vector<unsigned char> *out) {
	using namespace KeyValDB;
	const int maxsublen = 255 + 5;
	std::vector<int> saves;
	std::vector<short> sublens;
	std::vector<unsigned char> minvs;
	saves.resize(len + 1, 0);
	sublens.resize(len, 0);
	minvs.resize(len, 0);
	size_t osize = out->size();
	for (int i = len - 3; i >= 0; i--) {
		int minv, maxv, maxsaves = saves[i+1];
		minv = maxv = translate(in[i]);
		for (int j = i+1; j <= i+maxsublen && j < len; j++) {
			int tj = translate(in[j]);
			int sublen = j - i + 1;
			if (tj > maxv) maxv = tj;
			if (tj < minv) minv = tj;
			if (maxv - minv < 16 && sublen >= 6) {
				int nsave = sublen - ((sublen + 1) / 2 + 3);
				if (nsave > 0 && nsave + saves[i+sublen] > maxsaves) {
					maxsaves = nsave + saves[i+sublen];
					sublens[i] = sublen;
					minvs[i] = minv;
				}
			}
			if (maxv - minv >= 16) break;
		}
	}
	// find minkey
	int count[256];
	unsigned char key = 0;
	memset(count, 0, sizeof(count));
	for (int i = 0; i < len; i++) {
		count[translate(in[i])]++;
	}
	for (int i = 0; i < 256; i++) {
		if (count[i] < count[key]) key = i;
	}
	out->push_back(key);

	// build;
	for (int i = 0; i < len;) {
		unsigned char ti = translate(in[i]);
		if (sublens[i] == 0) {
			out->push_back(ti);
			if (ti == key) out->push_back(0);
			i++;
		} else {
			out->push_back(key);
			out->push_back(sublens[i] - 5);
			out->push_back(minvs[i]);
			for (int j = 0; j < sublens[i]; j+=2) {
				unsigned char tj = translate(in[i+j]) - minvs[i];
				unsigned char tj2 = (j == sublens[i]-1) ? 
					                0 : translate(in[i+j+1]) - minvs[i];
				out->push_back(tj | (tj2<<4));
			}
			i += sublens[i];
		}
	}
	return out->size() - osize;
}

int KeyValDB_decompress1(unsigned char *in, int len, std::vector<char> *decode) 
{
	using namespace KeyValDB;
	size_t osize = decode->size();
	unsigned char key = in[0];
	for (int i = 1; i < len;) {
		unsigned char c = in[i++];
		if (c != key) {
			decode->push_back(untranslate(c)); 
		} else {
			if (i >= len) break;
			unsigned char sublen = in[i++];
			if (sublen == 0) {
				decode->push_back(untranslate(key)); continue;
			}
			sublen += 5;
			if (i >= len) break;
			unsigned char minv = in[i++];
			for (int j = 0; j < sublen; j+=2) {
				if (i >= len) break;
				c = in[i++];
				decode->push_back(untranslate(minv + (c & 15)));
				if (j < sublen-1)
					decode->push_back(untranslate(minv + (c >> 4)));
			}
		}
	}
	return decode->size() - osize;
}

int KeyValDB_compress(const unsigned char *in, int len, 
					   std::vector<unsigned char> *out, void *memory) {
//	typedef int (*compFn)(const unsigned char *, int, std::vector<unsigned char> *);
//	static const compFn compFns[] = {&KeyValDB_compress0, &KeyValDB_compress1};

	//std::vector<unsigned char> tmp;
	//int r = KeyValDB_compress0(in, len, &tmp);
	//if (r <= 0) return r;
	//return KeyValDB_compress1(&(tmp[0]), r, out);
	return KeyValDB_compress0(in, len, out, memory);
}

int KeyValDB_decompress(unsigned char *in, int len, std::vector<char> *decode) 
{
	//std::vector<char> tmp;
	//int r = KeyValDB_decompress1(in, len, &tmp);
	//if (r <=0) return r;
	//return KeyValDB_decompress0((unsigned char*)&(tmp[0]), r, decode);
	return KeyValDB_decompress0(in, len, decode);
}

int KeyValDB_compressTest(const char *data, size_t len, const char *outfile)
{
	using namespace KeyValDB;
	//for (int i = 0; i <= 256; i++) {
	//	if (i == 256) {
	//		printf("translate OK\n"); break;
	//	}
	//	if (untranslate(translate(i)) != i) {
	//		printf("translate error %d->%d->%d\n", 
	//			   (int)i, (int)translate(i),
	//			   (int)untranslate(translate(i)));
	//		break;
	//	}
	//}
	if (!len) return 0;
	std::vector<unsigned char> compData;
	std::vector<char> decompData;
	compData.reserve(len + 1);
	int compSize = KeyValDB_compress((const unsigned char *)data, len, &compData, 0);
	if (!compSize) return -1;
	int decompSize = KeyValDB_decompress(&(compData[0]), compSize, &decompData);
	if (decompSize != len) return -2;
	if (memcmp(data, &(decompData[0]), len)) return -3;
	if (outfile) {
		FILE *f = fopen(outfile, "wb");
		if (f) {
			fwrite(&(compData[0]), 1, compData.size(), f);
			fclose(f);
		}
	}
	return compSize;
}
