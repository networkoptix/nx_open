#pragma once

#include <memory>

#include "socket_common.h"

namespace nx {
namespace network {

struct NX_NETWORK_API SystemSocketAddress
{
    std::shared_ptr<sockaddr> ptr;
    socklen_t size;

    SystemSocketAddress();
    SystemSocketAddress(int ipVersion);
    SystemSocketAddress(SocketAddress address, int ipVersion);

    SocketAddress toSocketAddress() const;
};

} // namespace network
} // namespace nx
