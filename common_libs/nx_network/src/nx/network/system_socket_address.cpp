#include "system_socket_address.h"

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include "socket_global.h"

namespace nx {
namespace network {

SystemSocketAddress::SystemSocketAddress():
    size(0)
{
}

SystemSocketAddress::SystemSocketAddress(int ipVersion):
    SystemSocketAddress(SocketAddress(), ipVersion)
{
}

SystemSocketAddress::SystemSocketAddress(SocketAddress endpoint, int ipVersion):
    SystemSocketAddress()
{
    if (SocketGlobals::ini().isHostDisabled(endpoint.address))
    {
        SystemError::setLastErrorCode(SystemError::noPermission);
        return;
    }

    if (ipVersion == AF_INET)
    {
        if (const auto ip = endpoint.address.ipV4())
        {
            const auto a = new sockaddr_in;
            memset(a, 0, sizeof(*a));
            ptr.reset((sockaddr*)a);
            size = sizeof(sockaddr_in);

            a->sin_family = AF_INET;
            a->sin_addr = *ip;
            a->sin_port = htons(endpoint.port);
            return;
        }
    }
    else if (ipVersion == AF_INET6)
    {
        const auto ipV6 = endpoint.address.ipV6();
        if (!(bool) ipV6.first)
            return;

        const auto a = new sockaddr_in6;
        memset(a, 0, sizeof(*a));
        ptr.reset((sockaddr*)a);
        size = sizeof(sockaddr_in6);

        a->sin6_flowinfo = 0;
        a->sin6_family = AF_INET6;
        a->sin6_addr = ipV6.first.get();
        a->sin6_port = htons(endpoint.port);

        if (endpoint.address.scopeId())
            a->sin6_scope_id = ipV6.second.get();
        else
            a->sin6_scope_id = 0;
    }

    SystemError::setLastErrorCode(SystemError::hostNotFound);
}

SocketAddress SystemSocketAddress::toSocketAddress() const
{
    if (!ptr)
        return SocketAddress();

    if (ptr->sa_family == AF_INET)
    {
        const auto a = reinterpret_cast<const sockaddr_in*>(ptr.get());
        return SocketAddress(a->sin_addr, ntohs(a->sin_port));
    }
    else if (ptr->sa_family == AF_INET6)
    {
        const auto a = reinterpret_cast<const sockaddr_in6*>(ptr.get());
        HostAddress resultHostAddr;
        if (a->sin6_scope_id  == 0)
            resultHostAddr = HostAddress(a->sin6_addr);
        else
            resultHostAddr = HostAddress(a->sin6_addr, a->sin6_scope_id);

        return SocketAddress(resultHostAddr, ntohs(a->sin6_port));
    }

    NX_ASSERT(false, lm("Corrupt family: %1").arg(ptr->sa_family));
    return SocketAddress();
}

} // namespace network
} // namespace nx
