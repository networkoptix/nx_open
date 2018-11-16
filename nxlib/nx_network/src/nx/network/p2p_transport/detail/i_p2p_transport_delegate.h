#pragma once

#include <nx/network/aio/abstract_async_channel.h>

namespace nx::network::detail {

class IP2PSocketDelegate: public aio::AbstractAsyncChannel
{
public:
    virtual SocketAddress getForeignAddress() const = 0;
};

} // namespace nx::network::detail