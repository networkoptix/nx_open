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

    const struct sockaddr* get() const;
    struct sockaddr* get();

    const socklen_t& length() const;
    socklen_t& length();

private:
    union
    {
        struct sockaddr untyped;
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } m_sockaddr;
    socklen_t m_sockaddrLen = 0;
};

} // namespace network
} // namespace nx
