#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <nx/network/async_stoppable.h>
#include <nx/network/aio/basic_pollable.h>
#include <nx/network/cloud/data/connect_data.h>
#include <nx/utils/move_only_func.h>

#include "abstract_outgoing_tunnel_connection.h"
#include "tunnel.h"

namespace nx {
namespace network {
namespace cloud {

/**
 * Creates outgoing specialized AbstractTunnelConnection.
 * NOTE: Methods of this class are not thread-safe.
 */
class NX_NETWORK_API AbstractTunnelConnector:
    public aio::BasicPollable
{
public:
    typedef nx::utils::MoveOnlyFunc<void(
        nx::hpm::api::NatTraversalResultCode resultCode,
        SystemError::ErrorCode sysErrorCode,
        std::unique_ptr<AbstractOutgoingTunnelConnection>)> ConnectCompletionHandler;

    /**
     * Helps to decide which method shall be used first.
     */
    virtual int getPriority() const = 0;
    /**
     * Establishes connection to the target host.
     * It is allowed to pipeline this method calls.
     * But these calls MUST be synchronized by caller.
     * @param timeout 0 means no timeout.
     * @param handler AbstractTunnelConnector can be safely freed within this handler.
     */
    virtual void connect(
        const hpm::api::ConnectResponse& response,
        std::chrono::milliseconds timeout,
        ConnectCompletionHandler handler) = 0;
    virtual const AddressEntry& targetPeerAddress() const = 0;
};

} // namespace cloud
} // namespace network
} // namespace nx
