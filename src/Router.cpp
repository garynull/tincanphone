/*
	(C) 2016 Gary Sinitsin. See LICENSE file (MIT license).
*/
#include "Router.h"
#include <cassert>
#include "miniupnpc/upnpcommands.h"
#include "miniupnpc/upnperrors.h"

namespace tincan {


Router::Router(int discoveryTimeout)
: upnpUrls(), //Zero init struct
  mappedPort(0)
{
	int error = 0;
	UPNPDev* devlist = upnpDiscover(discoveryTimeout, NULL, NULL, UPNP_LOCAL_PORT_ANY, 0, 2, &error);

	if (devlist)
	{
		// See header/source for UPNP_GetValidIGD for more info (yay magic constants!)
		int ret = UPNP_GetValidIGD(devlist, &upnpUrls, &upnpData, localAddr, sizeof(localAddr));
		freeUPNPDevlist(devlist);

		if (ret == -1) //UPNP_GetValidIGD returns -1 if it fails to allocate memory
		{
			throw std::bad_alloc();
		}
		else if (ret != 1) //1 means: A valid connected IGD has been found
		{
			FreeUPNPUrls(&upnpUrls);
			throw std::runtime_error("Router is not connected");
		}

		// We have a valid IGD, now get wanAddr for the win
		wanAddr[0] = '\0';
		error = UPNP_GetExternalIPAddress(upnpUrls.controlURL, upnpData.first.servicetype, wanAddr);
		if (error)
		{
			FreeUPNPUrls(&upnpUrls);
			throw std::runtime_error("UPNP_GetExternalIPAddress error " + toString(error));
		}
	}
	else
	{
		if (!error)
			throw std::runtime_error("Could not find a router with UPnP");
		else if (error == UPNPDISCOVER_MEMORY_ERROR)
			throw std::bad_alloc();
		else		
			throw std::runtime_error("upnpDiscover error " + toString(error));
	}
}

Router::~Router()
{
	try {
		clearPortMapping();
	} catch (...) {
		// Discard exceptions
	}

	FreeUPNPUrls(&upnpUrls);
}

bool Router::setPortMapping(uint16 localPort, uint16 wanPort, MapProto protocol, const char* descript)
{
	assert(protocol == MAP_TCP || protocol == MAP_UDP);
	assert(localPort >= 1024 && wanPort >= 1024);

	/*
	AddPortMapping - http://upnp.org/specs/gw/UPnP-gw-WANIPConnection-v2-Service.pdf
	"This action creates a new port mapping or overwrites an existing mapping with the same internal client.
	If the ExternalPort and PortMappingProtocol pair is already mapped to another internal client,
	an error is returned."

	Thus, AddPortMapping appears to return no error if you're just re-establishing an identical mapping.
	*/

	int error = UPNP_AddPortMapping(upnpUrls.controlURL, upnpData.first.servicetype,
								    toString(wanPort).c_str(), toString(localPort).c_str(), localAddr, descript,
								    getProtoStr(protocol), NULL, NULL);

	if (error == 718)
	{
		// 718 ConflictInMappingEntry
		// The port mapping entry specified conflicts with a mapping assigned previously to another client.
		return false;
	}
	else if (error)
	{
		throw std::runtime_error("UPNP_AddPortMapping error " + toString(error) + " (" + strupnperror(error) + ")");
	}

	mappedPort = wanPort;
	mappedProto = protocol;

	return true;
}

void Router::clearPortMapping()
{
	if (!mappedPort)
		return;

	// DeletePortMapping
	// UPnP Errors: 400-699, 402 "Invalid Args", 606 "Action not authorized", 714 "NoSuchEntryInArray"

	int error = UPNP_DeletePortMapping(upnpUrls.controlURL, upnpData.first.servicetype,
									   toString(mappedPort).c_str(),
									   getProtoStr(mappedProto), NULL);

	if (error && error != 714) //Ignore error 714 "NoSuchEntryInArray"
	{
		throw std::runtime_error("UPNP_DeletePortMapping error " + toString(error) + " (" + strupnperror(error) + ")");
	}

	mappedPort = 0;
}



}
