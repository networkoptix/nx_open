// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_socket_address.h"

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include "socket_global.h"

namespace nx::network {

SystemSocketAddress::SystemSocketAddress()
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
    // TODO: #akolesnikov Following check does not belong here.
    if (SocketGlobals::instance().isHostBlocked(endpoint.address))
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
        const auto ipV6 = endpoint.address.ipV6();
        if (!(bool) ipV6.first)
            return;

        m_sockaddrLen = sizeof(m_sockaddr.v6);

        m_sockaddr.v6.sin6_flowinfo = 0;
        m_sockaddr.v6.sin6_family = AF_INET6;
        m_sockaddr.v6.sin6_addr = *ipV6.first;
        m_sockaddr.v6.sin6_port = htons(endpoint.port);

        if (endpoint.address.scopeId())
            m_sockaddr.v6.sin6_scope_id = *ipV6.second;
        else
            m_sockaddr.v6.sin6_scope_id = 0;
    }

    SystemError::setLastErrorCode(SystemError::hostNotFound);
}

SocketAddress SystemSocketAddress::toSocketAddress() const
{
    if (m_sockaddrLen == 0)
        return SocketAddress();

    if (m_sockaddr.untyped.sa_family == AF_INET)
        return SocketAddress(m_sockaddr.v4);
    else if (m_sockaddr.untyped.sa_family == AF_INET6)
        return SocketAddress(m_sockaddr.v6);

    NX_ASSERT(false, nx::format("Unknown socket family type: %1").arg(m_sockaddr.untyped.sa_family));
    return SocketAddress();
}

const struct sockaddr* SystemSocketAddress::get() const
{
    return m_sockaddrLen == 0 ? nullptr : &m_sockaddr.untyped;
}

struct sockaddr* SystemSocketAddress::get()
{
    return m_sockaddrLen == 0 ? nullptr : &m_sockaddr.untyped;
}

const socklen_t& SystemSocketAddress::length() const
{
    return m_sockaddrLen;
}

socklen_t& SystemSocketAddress::length()
{
    return m_sockaddrLen;
}

} // namespace nx::network
