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
        if (const auto ip = endpoint.address.ipV6())
        {
            const auto a = new sockaddr_in6;
            memset(a, 0, sizeof(*a));
            ptr.reset((sockaddr*)a);
            size = sizeof(sockaddr_in6);

            a->sin6_family = AF_INET6;
            a->sin6_addr = *ip;
            a->sin6_port = htons(endpoint.port);
            return;
        }
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
        return SocketAddress(a->sin6_addr, ntohs(a->sin6_port));
    }

    NX_ASSERT(false, lm("Corupt family: %1").arg(ptr->sa_family));
    return SocketAddress();
}

} // namespace network
} // namespace nx
