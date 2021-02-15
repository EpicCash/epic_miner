#pragma once

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601 /* Windows 7 */
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
// this comment disable clang include reordering for windows.h
#include <windows.h>

inline void sock_init()
{
	static bool bWSAInit = false;

	if(!bWSAInit)
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		bWSAInit = true;
	}
}

inline void sock_shutdown(SOCKET s)
{
	shutdown(s, SD_BOTH);
}

inline void sock_close(SOCKET s)
{
	closesocket(s);
}

inline const char* sock_strerror(char* buf, size_t len)
{
	buf[0] = '\0';

	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)buf, len, NULL);

	return buf;
}

inline const char* sock_gai_strerror(int err, char* buf, size_t len)
{
	buf[0] = '\0';

	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL, (DWORD)err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)buf, len, NULL);

	return buf;
}

#else

/* Assume that any non-Windows platform uses POSIX-style sockets instead. */
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>		/* Needed for getaddrinfo() and freeaddrinfo() */
#include <netinet/in.h> /* Needed for IPPROTO_TCP */
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h> /* Needed for close() */

inline void sock_init() {}
typedef int SOCKET;

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)

inline void sock_shutdown(SOCKET s)
{
	shutdown(s, SHUT_RDWR);
}

inline void sock_close(SOCKET s)
{
	close(s);
}

inline const char* sock_strerror(char* buf, size_t len)
{
	buf[0] = '\0';

#if defined(__APPLE__) || defined(__FreeBSD__) || !defined(_GNU_SOURCE) || !defined(__GLIBC__)

	strerror_r(errno, buf, len);
	return buf;
#else
	return strerror_r(errno, buf, len);
#endif
}

inline const char* sock_gai_strerror(int err, char* buf, size_t len)
{
	buf[0] = '\0';
	return gai_strerror(err);
}
#endif
