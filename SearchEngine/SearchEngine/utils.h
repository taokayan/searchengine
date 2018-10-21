
#pragma once

#include <vector>
#include <Windows.h>
#include <emmintrin.h>

//#define USE_AVX

namespace Utils {

inline int nCPUs() {
	SYSTEM_INFO sysinfo;
	::GetSystemInfo( &sysinfo );
	int r = sysinfo.dwNumberOfProcessors;
	if (r < 1) r = 1;
	if (r > 8) r = 8;
	return r;
}

inline size_t nFreeRAM() {
	MEMORYSTATUSEX statex;
	statex.dwLength = sizeof (statex);
	if (::GlobalMemoryStatusEx (&statex)) {
		return statex.ullAvailPhys;
	}
	size_t min = 1048576;
	void *p = 0;
	do {
		p = ::malloc(min);
		if (!p) return min;
		::free(p);
		min *= 2;
		// don't alloc too much
		if (min >= 1048576 * 64) break; 
	} while(true);
	return min;
}

inline long long findchar(const char *str, size_t len, char c) {

#ifdef USE_AVX
	typedef __m256i VT;
#else 
	typedef __m128i VT;
#endif
	const char *str0 = str;
	const char *end = str + len;
	while ((unsigned long long)str & (sizeof(VT) - 1)) {
		if (str < end && str[0] == c) return str - str0;
		str++;
	}
	VT vc;
	memset(&vc, c, sizeof(vc));
	while (str < end - sizeof(VT) + 1) {
		VT vs = *(VT *)str;
#ifdef USE_AVX
		VT r = _mm256_cmpeq_epi8(vs, vc);
		if ((r.m256i_i64[0] | r.m256i_i64[1] | r.m256i_i64[2] | r.m256i_i64[3])) {
			if (!(r.m256i_i64[0] | r.m256i_i64[1])) {
				str += 16;
			}
			goto found;
		}
#else
		VT r = _mm_cmpeq_epi8(vs, vc);
		if ((r.m128i_i64[0] | r.m128i_i64[1])) goto found;
#endif
		str += sizeof(vs);
	}
	return -1;
found:
	do {
		if (str[0] == c) return str - str0;
		str++;
	} while (str < end); 
	return -1;
}

inline long long findchar2(const char *str, size_t len, char c) {
	const char *str0 = str;
	const char *end = str + len;
	while ((unsigned long long)str & 7) {
		if (str < end && str[0] == c) return str - str0;
		str++;
	}
	unsigned long long c_ = (unsigned char)c;
	c_ |= (c_ << 32);
	c_ |= (c_ << 16);
	c_ |= (c_ << 8);
	while (str < end) {
		unsigned long long mc = *(unsigned long long*)str;
		mc ^= c_; // check if mc contain 0 in one byte
		unsigned long long mc2 = mc & 0x7f7f7f7f7f7f7f7full;
		mc2 += 0x7f7f7f7f7f7f7f7full;
		mc2 &= 0x8080808080808080ull;
		if (mc2 == 0x8080808080808080ull) { str += 8; continue; }
		mc &= 0x8080808080808080ull;
		mc2 |= mc;
		if (mc2 == 0x8080808080808080ull) { str += 8; continue; }
		goto found;
	}
	return -1;
found:
	do {
		if (str[0] == c) return str - str0;
		str++;
	} while (str < end); 
	return -1;
}

inline int str_convert(const char *str1, int len, char *str2, int len2, int codePageFrom, int codePageTo) 
{
	int parsed = 0, written = 0;
	std::vector<wchar_t> utf16Str;
	int utf16written = 0;
	utf16Str.resize(2 * len + 4);
	utf16written = ::MultiByteToWideChar(codePageFrom, 
		                                 0, str1, len,
										 &(utf16Str[0]), utf16Str.size());
	if (utf16written <= 0) return written;

	int destwritten = ::WideCharToMultiByte(codePageTo, 0, &(utf16Str[0]), utf16written, 
		                                    str2, len2 - written, 0, 0);
	if (destwritten <= 0) return written;
	return written + destwritten;
}

inline int multiByteToUTF8(const char *mbStr, int len, char *utf8Str, int len2, int codePageFrom = CP_OEMCP) 
{
	return str_convert(mbStr, len, utf8Str, len2, codePageFrom, CP_UTF8);
}
inline int utf8ToMultiByte(const char *utf8Str, int len, char *mbStr, int len2, int codePageTo = CP_OEMCP) 
{
	return str_convert(utf8Str, len, mbStr, len2, CP_UTF8, codePageTo);
}
inline std::string multiByteToUTF8(const char *s, int lenIn, int codePage = CP_OEMCP) 
{
	std::vector<char> buf;
	buf.resize(lenIn * 4 + 4);
	int len = str_convert(s, lenIn, &(buf[0]), buf.size(), codePage, CP_UTF8);
	if (len <= 0) return std::string();
	return std::string(&(buf[0]), len);
}
inline std::string utf8ToMultiByte(const char *s, int lenIn, int codePage = CP_OEMCP) 
{
	std::vector<char> buf;
	buf.resize(lenIn * 4 + 4);
	int len = str_convert(s, lenIn, &(buf[0]), buf.size(), CP_UTF8, codePage);
	if (len <= 0) return std::string();
	return std::string(&(buf[0]), len);
}

}
