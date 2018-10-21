
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

class KKBufferReader {
	FILE   *m_file;
	size_t m_offset;
	size_t m_len;
	size_t m_bufSize;
	char   *m_buf;
	
public:
	KKBufferReader(FILE *f, size_t bufSize = 1048576 * 16): 
		           m_file(f), m_offset(0), m_len(0), m_bufSize(bufSize) {
		m_buf = (char *)malloc(bufSize);
	}
	~KKBufferReader() {
		if (m_buf) ::free(m_buf);
	}
	inline size_t seekRead(size_t offset, char *dest, size_t itemSize, size_t itemCount) {
		size_t len = itemSize * itemCount;
		size_t end = offset + len;
		if (offset >= m_offset && end <= m_offset + m_len) {
			memcpy(dest, m_buf + offset - m_offset, len);
			return itemCount;
		} else {
			::_fseeki64(m_file, offset, SEEK_SET);
			if (len > m_bufSize) {
				return ::fread(dest, itemSize, itemCount, m_file);
			} else {
				m_offset = offset;
				m_len = ::fread(m_buf, 1, m_bufSize, m_file);
				if (itemCount * itemSize > m_len) itemCount = m_len / itemSize;
				if (itemCount) memcpy(dest, m_buf, itemCount * itemSize);
				return itemCount;
			}
		}
	}
};
