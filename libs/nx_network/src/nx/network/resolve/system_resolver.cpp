#include "system_resolver.h"

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/system_error.h>

namespace nx {
namespace network {

namespace {

static std::deque<HostAddress> convertAddrInfo(addrinfo* addressInfo)
{
    std::deque<HostAddress> ipAddresses;
    for (addrinfo* info = addressInfo; info; info = info->ai_next)
    {
        HostAddress newAddress;
        switch (info->ai_family)
        {
            case AF_INET:
                newAddress = HostAddress(((sockaddr_in*)(info->ai_addr))->sin_addr);
                break;

            case AF_INET6:
                newAddress = HostAddress(
                    ((sockaddr_in6*)(info->ai_addr))->sin6_addr,
                    ((sockaddr_in6*)(info->ai_addr))->sin6_scope_id);
                break;

            default:
                continue;
        }

        // There may be more than one service on a single node, no reason to provide
        // duplicates as we do not provide any extra information about the node.
        if (std::find(ipAddresses.begin(), ipAddresses.end(), newAddress) == ipAddresses.end())
            ipAddresses.push_back(std::move(newAddress));
    }

    if (ipAddresses.empty())
        SystemError::setLastErrorCode(SystemError::hostNotFound);

    return ipAddresses;
}

static void convertAddrInfo(
    addrinfo* addressInfo,
    std::deque<AddressEntry>* resolvedEntries)
{
    auto resolvedAddresses = convertAddrInfo(addressInfo);

    for (auto& address: resolvedAddresses)
    {
        resolvedEntries->push_back(
            AddressEntry(AddressType::direct, std::move(address)));
    }
}

} // namespace

SystemError::ErrorCode SystemResolver::resolve(
    const QString& hostNameOriginal,
    int ipVersion,
    std::deque<AddressEntry>* resolvedEntries)
{
    auto resultCode = SystemError::noError;
    const auto guard = nx::utils::makeScopeGuard([&]() { SystemError::setLastErrorCode(resultCode); });
    QString hostName = hostNameOriginal.trimmed();
    if (hostName.isEmpty())
    {
        resultCode = SystemError::invalidData;
        return resultCode;
    }

    // Linux cannot resolve [ipv6 address].
    if (hostName.startsWith('[') && hostName.endsWith(']'))
        hostName = hostName.mid(1, hostName.size() - 2);

    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_ALL;
    hints.ai_family = AF_UNSPEC;
    // Do not search for IPv6 addresses on IpV4 sockets.
    if (ipVersion == AF_INET)
        hints.ai_family = ipVersion;

    addrinfo* addressInfo = nullptr;
    NX_VERBOSE(this, lm("Resolving %1 on DNS").arg(hostName));
    int status = getaddrinfo(hostName.toLatin1(), 0, &hints, &addressInfo);

    if (status == EAI_BADFLAGS)
    {
        // If the lookup failed with AI_ALL, try again without it.
        hints.ai_flags = 0;
        status = getaddrinfo(hostName.toLatin1(), 0, &hints, &addressInfo);
    }

    if (status != 0)
    {
        resultCode = resolveStatusToErrno(status);

        NX_VERBOSE(this, lm("Resolve of %1 on DNS failed with result %2")
            .arg(hostName).arg(SystemError::toString(resultCode)));
        return resultCode;
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addressInfoGuard(
        addressInfo, &freeaddrinfo);

    convertAddrInfo(addressInfo, resolvedEntries);
    if (resolvedEntries->empty())
    {
        resultCode = SystemError::hostNotFound;
        NX_VERBOSE(this, lm("Resolve of %1 on DNS failed with result %2. No suitable entries")
            .arg(hostName).arg(SystemError::toString(resultCode)));
        return resultCode;
    }

    if (ipVersion == AF_INET6)
        ensureLocalHostCompatibility(hostNameOriginal, resolvedEntries);

    NX_VERBOSE(this, lm("Resolve of %1 on DNS completed successfully. %2 entries found")
        .args(hostName, resolvedEntries->size()));

    return SystemError::noError;
}

SystemError::ErrorCode SystemResolver::resolveStatusToErrno(int status)
{
    switch (status)
    {
        case EAI_NONAME:
            return SystemError::hostNotFound;
        case EAI_AGAIN:
            return SystemError::again;
        case EAI_MEMORY:
            return SystemError::noMemory;

#if defined(__linux__)
        case EAI_SYSTEM:
            return SystemError::getLastOSErrorCode();
#endif

        default:
            return SystemError::dnsServerFailure;
    };
}

void SystemResolver::ensureLocalHostCompatibility(
    const QString& hostName,
    std::deque<AddressEntry>* resolvedAddresses)
{
    if (hostName != "localhost")
        return;

    // On Linux, localhost is resolved to 127.0.0.1 only, but ::1 can still be used to connect to.
    // On Windows, localhost is resolved to (127.0.01, ::1).
    // Making behavior consistent across different platforms.

    auto ipv4ResolvedLocalhost =
        std::find_if(
            resolvedAddresses->begin(), resolvedAddresses->end(),
            [](const AddressEntry& entry)
            {
                return entry.host.ipV4() && entry.host.ipV4()->s_addr == htonl(INADDR_LOOPBACK);
            });

    auto ipv6ResolvedLocalhost =
        std::find_if(
            resolvedAddresses->begin(), resolvedAddresses->end(),
            [](const AddressEntry& entry)
            {
                return entry.host.ipV6().first && *entry.host.ipV6().first == in6addr_loopback;
            });

    if (ipv4ResolvedLocalhost != resolvedAddresses->end() &&
        ipv6ResolvedLocalhost == resolvedAddresses->end())
    {
        resolvedAddresses->push_back(AddressEntry(
            AddressType::direct, HostAddress(in6addr_loopback, 0)));
    }
}

} // namespace network
} // namespace nx
