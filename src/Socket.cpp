/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#include "Socket.h"
#include <cassert>

#ifndef _WIN32
#	include <errno.h>
#	include <fcntl.h>
#endif

namespace tincan {


#ifdef _WIN32

int Socket::getError()
{
	return WSAGetLastError();
}

int Socket::close(SOCKET s)
{
	return closesocket(s);
}

void Socket::setBlocking(SOCKET s, bool blocking)
{
	ulong b = !blocking;
	if (ioctlsocket(s, FIONBIO, &b) == SOCKET_ERROR)
		throw std::runtime_error("Failed setting socket to non-blocking");
}

#else

int Socket::getError()
{
	return errno;
}

int Socket::close(SOCKET s)
{
	return ::close(s);
}

void Socket::setBlocking(SOCKET s, bool blocking)
{
	int flags = fcntl(s, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("F_GETFL failed");
	
	if (!blocking)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;
	
	if (fcntl(s, F_SETFL, flags) == -1)
		throw std::runtime_error("Failed setting socket to non-blocking");
}

#endif


// This suits our purposes while keeping things simple
string Socket::getErrorString()
{
	switch (getError())
	{
	case EWOULDBLOCK:
		return "EWOULDBLOCK";
	case EADDRINUSE:
		return "EADDRINUSE";
	case ECONNABORTED:
		return "ECONNABORTED";
	case ECONNRESET:
		return "ECONNRESET";
	}
	return toString(getError());
}


std::ostream& operator << (std::ostream& os, const sockaddr_storage& rhs)
{
	char host[NI_MAXHOST];
	char port[NI_MAXSERV];

	assert(rhs.ss_family == AF_INET || rhs.ss_family == AF_INET6);
	const socklen_t size = (rhs.ss_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
	int e = getnameinfo((const sockaddr*)&rhs, size, host, sizeof(host), port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);
	if (e)
		throw std::runtime_error("getnameinfo failed with error code " + toString(e));
	
	os << host << ':' << port;

	return os;
}

bool operator == (const sockaddr_storage& lhs, const sockaddr_storage& rhs)
{
	if (lhs.ss_family != rhs.ss_family)
		return false;

	assert(rhs.ss_family == AF_INET || rhs.ss_family == AF_INET6);
	const size_t size = (rhs.ss_family == AF_INET) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

	return memcmp(&lhs, &rhs, size) == 0;
}


}
