#pragma once

#include <memory>
#include <nx/network/aio/abstract_async_channel.h>
#include <nx/network/http/http_types.h>

namespace nx::p2p {

class IP2PTransport: public network::aio::AbstractAsyncChannel
{
public:
    virtual ~IP2PTransport() = default;
    virtual network::SocketAddress getForeignAddress() const = 0;
    virtual void start(utils::MoveOnlyFunc<void(SystemError::ErrorCode)> onStart = nullptr) = 0;
};

using P2pTransportPtr = std::unique_ptr<IP2PTransport>;

} // namespace nx::network