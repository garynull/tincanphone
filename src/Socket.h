/*
	(C) 2016 Gary Sinitsin. See LICENSE.txt (MIT license).
*/
#pragma once

#include "PhoneCommon.h"

#ifdef _WIN32
#	define UNICODE
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <Windows.h>
#	include <WinSock2.h>
#	include <WS2tcpip.h>
	// Bring back the standard error constants that we use
#	define EWOULDBLOCK      WSAEWOULDBLOCK
#	define EADDRINUSE       WSAEADDRINUSE
#	define ECONNABORTED     WSAECONNABORTED
#	define ECONNRESET       WSAECONNRESET
#else
#	include <unistd.h>
#	include <errno.h>
#	include <sys/types.h>
#	include <sys/socket.h>
#	include <netdb.h>
	typedef int SOCKET;     //Since Winsock requires SOCKET type for socket fds
#endif

namespace tincan {


// A super-thin wrapper around the stuff Winsock messes up
class Socket
{
public:
	static int    getError();
	static string getErrorString();
	static int    close(SOCKET s);
	static void   setBlocking(SOCKET s, bool blocking);
};


// Conveniences for using socket structs with C++
std::ostream& operator << (std::ostream& os, const sockaddr_storage& rhs);
bool          operator == (const sockaddr_storage& lhs, const sockaddr_storage& rhs);
inline bool   operator != (const sockaddr_storage& lhs, const sockaddr_storage& rhs)  {return !(lhs == rhs);}


}
