#include "system_resolver.h"

#include <nx/utils/log/log.h>

#include <utils/common/guard.h>
#include <utils/common/systemerror.h>

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
                newAddress = HostAddress(((sockaddr_in6*)(info->ai_addr))->sin6_addr);
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

    return std::move(ipAddresses);
}

} // namespace

std::deque<HostAddress> SystemResolver::resolve(const QString& hostName, int ipVersion)
{
    auto resultCode = SystemError::noError;
    const auto guard = makeScopedGuard([&]() { SystemError::setLastErrorCode(resultCode); });
    if (hostName.isEmpty())
    {
        resultCode = SystemError::invalidData;
        return std::deque<HostAddress>();
    }

    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_flags = AI_ALL;
    hints.ai_family = AF_UNSPEC;
    // Do not search for IPv6 addresses on IpV4 sockets.
    if (ipVersion == AF_INET)
        hints.ai_family = ipVersion;

    addrinfo* addressInfo = nullptr;
    NX_LOGX(lm("Resolving %1 on DNS").str(hostName), cl_logDEBUG2);
    int status = getaddrinfo(hostName.toLatin1(), 0, &hints, &addressInfo);

    if (status == EAI_BADFLAGS)
    {
        // If the lookup failed with AI_ALL, try again without it.
        hints.ai_flags = 0;
        status = getaddrinfo(hostName.toLatin1(), 0, &hints, &addressInfo);
    }

    if (status == 0)
    {
        NX_LOGX(lm("Resolve of %1 on DNS completed successfully").str(hostName), cl_logDEBUG2);
    }
    else
    {
        switch (status)
        {
            case EAI_NONAME: resultCode = SystemError::hostNotFound; break;
            case EAI_AGAIN: resultCode = SystemError::again; break;
            case EAI_MEMORY: resultCode = SystemError::nomem; break;

            #if defined(__linux__)
            case EAI_SYSTEM: resultCode = SystemError::getLastOSErrorCode(); break;
            #endif

                // TODO: #mux Translate some other status codes?
            default: resultCode = SystemError::dnsServerFailure; break;
        };

        NX_LOGX(lm("Resolve of %1 on DNS failed with result %2")
            .str(hostName).str(SystemError::toString(resultCode)), cl_logDEBUG2);
        return std::deque<HostAddress>();
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addressInfoGuard(addressInfo, &freeaddrinfo);
    const auto result = convertAddrInfo(addressInfo);
    if (result.empty())
        resultCode = SystemError::hostNotFound;

    return result;
}

} // namespace network
} // namespace nx
