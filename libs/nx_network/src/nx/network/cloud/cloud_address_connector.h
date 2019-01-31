#pragma once

#include <chrono>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_attributes_cache.h>
#include <nx/utils/async_operation_guard.h>
#include <nx/utils/move_only_func.h>

#include "tunnel/outgoing_tunnel.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Invokes OutgoingTunnelPool::establishNewConnection to establish new cloud connection.
 */
class NX_NETWORK_API CloudAddressConnector:
    public aio::BasicPollable
{
    using base_type = aio::BasicPollable;

public:
    using ConnectHandler = OutgoingTunnel::NewConnectionHandler;

    /**
     * @param timeout Zero means no timeout.
     */
    CloudAddressConnector(
        const AddressEntry& targetHostAddress,
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes);
    ~CloudAddressConnector();

    void connectAsync(ConnectHandler handler);

protected:
    virtual void stopWhileInAioThread() override;

private:
    const AddressEntry m_targetHostAddress;
    const std::chrono::milliseconds m_timeout;
    SocketAttributes m_socketAttributes;
    nx::utils::AsyncOperationGuard m_asyncConnectGuard;
    ConnectHandler m_handler;
};

} // namespace cloud
} // namespace network
} // namespace nx
