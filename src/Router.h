/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#pragma once

#include "PhoneCommon.h"
#include "miniupnpc/miniupnpc.h"

namespace tincan {


// Represents a router (or other such NAT device) accessible via UPnP
// This class uses miniupnpc internally
class Router
{
public:
	Router(int discoveryTimeout);
	~Router();

	// Returns the local IP as reported by UPnP
	string getLocalAddress() const  {return localAddr;}

	// Returns the public IP as reported by UPnP
	string getWanAddress() const  {return wanAddr;}

	enum MapProto { MAP_TCP, MAP_UDP };

	// Returns TRUE on success, FALSE if port in use, throws if error
	bool setPortMapping(uint16 localPort, uint16 wanPort, MapProto protocol, const char* descript);

	// Clears previously set port mapping if any, throws if error
	void clearPortMapping();

protected:
	UPNPUrls upnpUrls;
	IGDdatas upnpData;
	char     localAddr[64];
	char     wanAddr[64];
	uint16   mappedPort;
	MapProto mappedProto;
	static const char* getProtoStr(MapProto proto)  {return (proto == MAP_TCP) ? "TCP" : "UDP";}
};


}
