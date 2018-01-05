#include "system_socket_address.h"

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include "socket_global.h"

namespace nx {
namespace network {

SystemSocketAddress::SystemSocketAddress():
    m_sockaddrLen(0)
{
    memset(&m_sockaddr, 0, sizeof(m_sockaddr));
}

SystemSocketAddress::SystemSocketAddress(int ipVersion):
    SystemSocketAddress(SocketAddress(), ipVersion)
{
}

SystemSocketAddress::SystemSocketAddress(SocketAddress endpoint, int ipVersion):
    SystemSocketAddress()
{
    // TODO: #ak Following check does not belong here.
    if (SocketGlobals::ini().isHostDisabled(endpoint.address))
    {
        SystemError::setLastErrorCode(SystemError::noPermission);
        return;
    }

    if (ipVersion == AF_INET)
    {
        if (const auto ip = endpoint.address.ipV4())
        {
            m_sockaddrLen = sizeof(m_sockaddr.v4);

            m_sockaddr.v4.sin_family = AF_INET;
            m_sockaddr.v4.sin_addr = *ip;
            m_sockaddr.v4.sin_port = htons(endpoint.port);
            return;
        }
    }
    else if (ipVersion == AF_INET6)
    {
        if (const auto ip = endpoint.address.ipV6())
        {
            m_sockaddrLen = sizeof(m_sockaddr.v6);

            m_sockaddr.v6.sin6_family = AF_INET6;
            m_sockaddr.v6.sin6_addr = *ip;
            m_sockaddr.v6.sin6_port = htons(endpoint.port);
            return;
        }
    }

    SystemError::setLastErrorCode(SystemError::hostNotFound);
}

SocketAddress SystemSocketAddress::toSocketAddress() const
{
    if (m_sockaddrLen == 0)
        return SocketAddress();

    if (m_sockaddr.untyped.sa_family == AF_INET)
        return SocketAddress(m_sockaddr.v4.sin_addr, ntohs(m_sockaddr.v4.sin_port));
    else if (m_sockaddr.untyped.sa_family == AF_INET6)
        return SocketAddress(m_sockaddr.v6.sin6_addr, ntohs(m_sockaddr.v6.sin6_port));

    NX_ASSERT(false, lm("Unknown socket family type: %1").arg(m_sockaddr.untyped.sa_family));
    return SocketAddress();
}

const struct sockaddr* SystemSocketAddress::addr() const
{
    return m_sockaddrLen == 0 ? nullptr : &m_sockaddr.untyped;
}

struct sockaddr* SystemSocketAddress::addr()
{
    return m_sockaddrLen == 0 ? nullptr : &m_sockaddr.untyped;
}

const socklen_t& SystemSocketAddress::addrLen() const
{
    return m_sockaddrLen;
}

socklen_t& SystemSocketAddress::addrLen()
{
    return m_sockaddrLen;
}

} // namespace network
} // namespace nx
