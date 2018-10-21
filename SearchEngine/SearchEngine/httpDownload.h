
#pragma once

#include <vector>
#include <stdlib.h>

#include "KKSocket.hpp"
#include "SearchEngine.h"

// receive content from socket
bool recvContent(KKSocket *client, std::vector<char> &data) {
	static const char str0[] = {"content-type:"};
	static const char str1[] = {"text/html"};
	static const char str2[] = {"html"};
	static const char str_html_end[] = { "</html>" };
	DWORD t = ::timeGetTime();
	DWORD expire = t + DOWNLOADTIMEOUT;
	int dt = 4;
	int parsed = 0;
	int step = 0;
	data.clear();
	data.reserve(65536);
	const int maxLength = 400 * 1024;
	const int maxHdrLen = 1500;
	int delayMult = 1;
	do {
		int tremain = expire - ::timeGetTime();
		if (tremain <= 0 || data.size() >= maxLength) break;
		int flag = client->wait(true, false, true, tremain / 1000, tremain % 1000 * 1000);
		int navail = client->nAvailRead();
		if (navail == 0) break; // closed
		if (navail <= 0) {
			if (flag & 4) break;
			if (::timeGetTime() < expire) {
				//::Sleep(100 + (rand() & 127)); 
				continue;
			}
			//::Sleep((10 + (rand() & 15)) * delayMult); // something wrong happens here...
			//if (delayMult < 8) delayMult++;
			//continue;
			break;
		}
		int osize = data.size();
		data.resize(data.size() + navail);
		int r = client->recv(&(data[osize]), navail);
		if (r <= 0) r = 0;
		data.resize(osize + r);
		if (step == 0) {
			for (;parsed < maxHdrLen && parsed < (int)data.size() - 32; parsed++) {
				if (step == 0 && memicmp(&data[parsed], str0, sizeof(str0)-1)==0) {
					parsed += sizeof(str0)-1;
					while (parsed < data.size() - 16 && data[parsed]== ' ') parsed++;
					if (memicmp(&data[parsed], str1, sizeof(str1)-1) == 0 ||
						memicmp(&data[parsed], str2, sizeof(str2)-1) == 0) {
						step++; // ok
						parsed += sizeof(str1);
						break;
					} else {
						return false; // falsed, not html
					}
				} 
			}
			if (step == 0 && parsed >= maxHdrLen) return false; // not html
		}
		else if (step == 1) {
			for (;parsed < (int)(data.size() - (sizeof(str_html_end)-1)); parsed++) {
				if (memicmp(&data[parsed], str_html_end, sizeof(str_html_end)-1)==0) {
					goto finished;
				}
			}
		}
	} while (true);
finished:
	return true;
}

bool httpDownload(KKSocket &client, const char *url, std::string host, std::vector<char> &data,
				  Model *model)
{
	in_addr ipv4;
	int port;
	char url2[MAXURLLEN + 4];
	int url2Len = 0;
	int retry = 1;
	char toSend[4096];
	int len;
again:
	len = sprintf(toSend, "GET %s HTTP/1.0\nHost: %s\n\n",
		          (url[0] ? url : "/index.html"),
			      host.c_str());
	client.setNonBlock(true);
	client.send(toSend, len);
	model->m_sentBytes += len;
	if (!recvContent(&client, data)) { 
		data.clear();
		return false;
	}
	char firstLine[256], col0[256];
	int code = 0;
	len = 0;
	while (len < data.size() && len < 255 && 
		   data[len] && data[len] != '\r' && data[len] != '\n') {
		firstLine[len] = data[len];
		len++;
	}
	firstLine[len] = 0;
	if (sscanf(firstLine, "%s%d", col0, &code) == 2) {
		if (code == 301) { // move permanently...
			if (!retry) {
				data.clear(); return false;
			}
			for (int i = 0; i < data.size() - 21 && i < 2048; i++) {
				if (!memicmp(&(data[i]), "Location:", 9)) {
					i += 9;
					while (i < data.size() && data[i] == ' ') i++;
					while (i < data.size() && isValidURLchar(data[i]) && url2Len < MAXURLLEN) {
						url2[url2Len++] = data[i++];
					}
					if (url2Len) {
						url2[url2Len] = 0;
						url = split(url2, host, port);
						data.clear();
						client.close();
						if (!client.connect(host.c_str(), port, &ipv4)) return false;
						retry = false;
						goto again;
					} else {
						data.clear();
						return false;
					}
				}
			}
			data.clear(); return false;
		}
		else if (code == 404) {
			data.clear(); return false;
		}
	}
	return true;
}
