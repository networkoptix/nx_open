#pragma once

#include <memory>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/http/http_types.h>

namespace nx::network {

class NX_NETWORK_API IP2PTransport: public aio::AbstractAsyncChannel
{
public:
    virtual ~IP2PTransport() = default;
    virtual SocketAddress getForeignAddress() const = 0;
    virtual void start(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) = 0;
};

using P2pTransportPtr = std::unique_ptr<IP2PTransport>;

} // namespace nx::network