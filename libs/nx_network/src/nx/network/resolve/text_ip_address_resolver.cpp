// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "text_ip_address_resolver.h"

namespace nx {
namespace network {

SystemError::ErrorCode TextIpAddressResolver::resolve(
    const std::string_view& hostName,
    int ipVersion,
    ResolveResult* resolveResult)
{
    HostAddress hostAddress(hostName);

    if (ipVersion == AF_INET && hostAddress.ipV4())
    {
        resolveResult->entries.push_back(
            {{ AddressEntry(AddressType::direct, *hostAddress.ipV4()) }});
        return SystemError::noError;
    }

    IpV6WithScope ipV6WithScope = hostAddress.ipV6();
    if (!ipV6WithScope.first || !hostAddress.isPureIpV6())
        return SystemError::hostUnreachable;

    resolveResult->entries.push_back({{ AddressEntry(
        AddressType::direct,
        HostAddress(*ipV6WithScope.first, ipV6WithScope.second)) }});
    return SystemError::noError;
}

} // namespace network
} // namespace nx
