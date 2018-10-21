
#pragma once

#include <vector>
#include <string>

// work for http & https
bool downloadEx(const char *host, int port, 
			    const char *path, std::vector<char> &outData,
			    size_t maxLen = INT_MAX); 
