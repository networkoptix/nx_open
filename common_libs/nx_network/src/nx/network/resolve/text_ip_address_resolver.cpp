#include "text_ip_address_resolver.h"

namespace nx {
namespace network {

SystemError::ErrorCode TextIpAddressResolver::resolve(
    const QString& hostName,
    int ipVersion,
    std::deque<AddressEntry>* resolvedAddresses)
{
    HostAddress hostAddress(hostName);

    if (ipVersion == AF_INET && hostAddress.ipV4())
    {
        resolvedAddresses->push_back(
            {{ AddressEntry(AddressType::direct, *hostAddress.ipV4()) }});
        return SystemError::noError;
    }

    IpV6WithScope ipV6WithScope = hostAddress.ipV6();
    if (!ipV6WithScope.first || !hostAddress.isPureIpV6())
        return SystemError::hostUnreachable;

    resolvedAddresses->push_back({{ AddressEntry(
        AddressType::direct,
        HostAddress(*ipV6WithScope.first, ipV6WithScope.second)) }});
    return SystemError::noError;
}

} // namespace network
} // namespace nx
