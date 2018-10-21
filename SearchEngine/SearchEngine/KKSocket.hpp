
#pragma once

#include "stdafx.h"
#include <string>

#include <WinSock.h>
#pragma comment(lib, "Ws2_32.lib")

inline void KKSocket_Startup() {
	WSADATA info;
	WSAStartup(MAKEWORD(2,0), &info);
}

class KKSocket
{
public:
	typedef SOCKET Socket;
private:
	KKSocket(const KKSocket&);
	KKSocket &operator=(const KKSocket&);
	Socket  m_sock;
	int     m_lastError;
public:
	inline KKSocket() {
		m_lastError = 0;
		m_sock = 0;
	}
	inline ~KKSocket() {
		if (m_sock) ::closesocket(m_sock);
	}
	inline void close() {
		Socket s = m_sock;
		m_sock = 0;
		if (s) ::closesocket(s);
	}
	inline Socket ptr() { return m_sock; }
	inline int lastError() const { return m_lastError; }
	inline operator bool() const { return (bool)m_sock; }
	inline bool udp() {
		if (m_sock) return false;
		m_sock = ::socket(AF_INET,SOCK_DGRAM,0);
		if (m_sock == INVALID_SOCKET) {
			m_sock = 0;
			m_lastError = WSAGetLastError();
			return false;
		}
		return true;
	}
	inline bool setNonBlock(bool enable) {
		if (!m_sock) return false;
		u_long mode = (enable ? 1 : 0);
		if (::ioctlsocket(m_sock, FIONBIO, &mode)) {
			m_lastError = WSAGetLastError();
			return false;
		}
		return true;
	}
	inline void setNoDelay(bool v) {
		if (!m_sock) return;
		BOOL b = v; 
		::setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, (char*)(&b), sizeof(BOOL));
	}
	inline size_t nAvailRead() {
		if (!m_sock) return 0;
		u_long bufferLen = 0;
        ::ioctlsocket(m_sock, FIONREAD, &bufferLen); 
		return bufferLen;
	}
	inline int recv(char *data, int len) { // return 0: socket closed, negative: error
		if (!m_sock) return -1;
		if (len <= 0) return -1;
		return ::recv(m_sock, data, len, 0);
	}
	inline int send(const char *data, int len) {
		if (!m_sock) return -1;
		if (len <= 0) return -1;
		int r;
		if ((r = ::send(m_sock, data, len, 0)) <= 0) {
			m_lastError = WSAGetLastError();
		}
		return r;
	}
	inline bool listen(int port, int connections) 
	{
		if (!m_sock) {
			m_sock = ::socket(AF_INET,SOCK_STREAM,0);
			if (m_sock == INVALID_SOCKET) {
				m_sock = 0;
				m_lastError = WSAGetLastError();
				return false;
			}
		}
		sockaddr_in sa;
		memset(&sa, 0, sizeof(sa));
		sa.sin_family = PF_INET;             
		sa.sin_port = htons(port);   

		if (::bind(m_sock, (sockaddr *)&sa, sizeof(sockaddr_in)) == SOCKET_ERROR) {
			m_lastError = WSAGetLastError();
			return false;
		}
		::listen(m_sock, connections);  
		return true;
	}
	inline bool connect(in_addr ipv4addr, int port) {
		if (!m_sock) {
			m_sock = ::socket(AF_INET,SOCK_STREAM,0);
			if (m_sock == INVALID_SOCKET) {
				m_sock = 0;
				m_lastError = WSAGetLastError();
				return false;
			}
		}
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr = ipv4addr;
		memset(&(addr.sin_zero), 0, 8); 
		if (::connect(m_sock, (sockaddr *) &addr, sizeof(sockaddr))) {
			m_lastError = WSAGetLastError();
			return false;
		}
		return true;
	}
	inline bool connect(const char *host, int port, in_addr *outAddr = 0) {
		hostent *he;
		if ((he = ::gethostbyname(host)) == 0) {
			if (outAddr) outAddr->S_un.S_addr = 0;
			m_lastError = -1;
			return false;
		}
		if (outAddr) *outAddr = *((in_addr *)he->h_addr);
		return connect(*((in_addr *)he->h_addr), port);
	}
	inline bool accept(KKSocket *s) {
		if (!m_sock || !s || s->m_sock) return false;
		Socket new_sock = ::accept(m_sock, 0, 0);
		if (new_sock == INVALID_SOCKET) {
			m_lastError = WSAGetLastError();
			return false;
		}
		s->m_sock = new_sock;
		return true;
	}
	// return flags: 1-read, 2-write, 4-exception/error
	inline int wait(bool read, bool write, bool error,
		             int timeoutsec, int timeoutusec) {
		if (!m_sock) return 0;
		setNonBlock(false);
		fd_set fd[3];
		for (int i = 0; i < 3; i++) {
			FD_ZERO(&fd[i]);
			FD_SET(m_sock, &fd[i]);
		}
		TIMEVAL tval;
		tval.tv_sec  = timeoutsec;
		tval.tv_usec = timeoutusec;
		if (::select(0, 
			         read ? &fd[0] : 0, write ? &fd[1] : 0, error ? &fd[2] : 0, 
					 &tval) == SOCKET_ERROR) {
			m_lastError = WSAGetLastError();
			return 0;
		}
		int res = 0;
		if (read && FD_ISSET(m_sock, &fd[0])) res |= 1;
		if (write && FD_ISSET(m_sock, &fd[1])) res |= 2;
		if (error && FD_ISSET(m_sock, &fd[2])) res |= 4;
		return res;
	}
};
