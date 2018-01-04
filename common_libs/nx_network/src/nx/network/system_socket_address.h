#pragma once

#include "socket_common.h"

namespace nx {
namespace network {

class NX_NETWORK_API SystemSocketAddress
{
public:
    SystemSocketAddress();
    SystemSocketAddress(int ipVersion);
    SystemSocketAddress(SocketAddress address, int ipVersion);

    SocketAddress toSocketAddress() const;

    const struct sockaddr* addr() const;
    struct sockaddr* addr();

    const socklen_t& addrLen() const;
    socklen_t& addrLen();

private:
    union
    {
        struct sockaddr untyped;
        sockaddr_in v4;
        sockaddr_in6 v6;
    } m_sockaddr;
    socklen_t m_sockaddrLen = 0;
};

} // namespace network
} // namespace nx
