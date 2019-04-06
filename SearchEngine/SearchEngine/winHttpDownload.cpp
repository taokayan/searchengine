
#include "stdafx.h"

#include "winHttpDownload.h"

#include <Windows.h>

#include <winhttp.h>
#include <schannel.h>

#include "..\..\Multiplexer\KKAtomic.hpp"

namespace {
	inline void towstr(const char *src, LPWSTR dest, size_t max) {
		int len = 0;
		while (*src && len < max - 1) {
			dest[len++] = *src; src++;
		}
		dest[len] = 0;
	}

	static KKAtomic<unsigned int> l_agentID = 0;
}

bool downloadEx(const char *host, int port, 
			    const char *path, std::vector<char> &outData,
			    size_t maxLen)
{
  try {
	  int navail = 0;
	  int nread = 0;
	  LPSTR pszOutBuffer;
	  BOOL  bResults = FALSE;
	  HINTERNET  hSession = NULL, 
				 hConnect = NULL,
				 hRequest = NULL;
	  WCHAR wHost[1024], wPath[2048];
	  WCHAR wAgent[32];
	  volatile int check = 0;

	  towstr(host, wHost, sizeof(wHost)/sizeof(wHost[0]));
	  towstr(path, wPath, sizeof(wPath)/sizeof(wPath[0]));

	  check = 1;

	  unsigned id = l_agentID++;
	  wsprintf(wAgent, L"winHttp %u", id);

	  // Use WinHttpOpen to obtain a session handle.
	  hSession = WinHttpOpen( wAgent,  
							  WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
							  WINHTTP_NO_PROXY_NAME, 
							  WINHTTP_NO_PROXY_BYPASS, 0 );

	  check = 2;
	  if (hSession) {
		WinHttpSetTimeouts( hSession, 4000, 4000, 4000, 10000);
	  }
	  check = 3;
	  // Specify an HTTP server.
	  if( hSession )
		hConnect = WinHttpConnect( hSession, wHost, port, 0 );
	  check = 4;
	  // Create an HTTP request handle.
	  if( hConnect )
		hRequest = WinHttpOpenRequest( hConnect, L"GET", wPath,
									   NULL, WINHTTP_NO_REFERER, 
									   WINHTTP_DEFAULT_ACCEPT_TYPES, 
									   port == 443 ? WINHTTP_FLAG_SECURE : 0);
	  check = 5;
	  // Send a request.
	  if( hRequest )
		bResults = WinHttpSendRequest( hRequest,
									   WINHTTP_NO_ADDITIONAL_HEADERS, 0,
									   WINHTTP_NO_REQUEST_DATA, 0, 
									   0, 0 );
	  check = 6;
	  // End the request.
	  if( bResults )
		bResults = WinHttpReceiveResponse( hRequest, NULL );
	  check = 7;
	  // Keep checking for data until there is nothing left.

	  if( bResults )
	  {
		size_t res_size = 16384;
		outData.reserve(res_size);
		do
		{
		  // Check for available data.
		  navail = 0;
		  if (!WinHttpQueryDataAvailable( hRequest, (LPDWORD)&navail)) break;
		  if (navail <= 0) break;

		  size_t osize = outData.size();
		  if (osize + navail > maxLen) {
			  navail = maxLen - osize;
		  }
		  if (navail <= 0) break;
		  navail += 4096;
		  outData.resize(osize + navail);
		  nread = 0;
		  if( !WinHttpReadData(hRequest, (LPVOID)&(outData[osize]), 
							   navail - 32, (LPDWORD)&nread) ) break;
		  if (nread < 0) nread = 0;
		  outData.resize(osize + nread);
		} while( navail > 0 );
		check = 8;
	  }

	  bool ret = true;
	  // Report any errors.
	  if( !bResults ) ret = false;

	  check = 9;
	  // Close any open handles.
	  if( hRequest ) WinHttpCloseHandle( hRequest );
	  check = 10;
	  if( hConnect ) WinHttpCloseHandle( hConnect );
	  check = 11;
	  if( hSession ) WinHttpCloseHandle( hSession );
	  check = 12;
	  return ret;
  } catch (...) {
	  return false;
  }
}
