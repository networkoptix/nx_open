#pragma once

#include <nx/network/aio/basic_pollable.h>
#include <nx/utils/move_only_func.h>

#include "relay_api_result_code.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {
namespace api {

class NX_NETWORK_API ClientToRelayConnection:
    public aio::BasicPollable
{
public:
    ClientToRelayConnection(const SocketAddress& relayEndpoint);
    virtual ~ClientToRelayConnection() override;

    void startSession(
        const nx::String& desiredSessionId,
        const nx::String& targetPeerName,
        nx::utils::MoveOnlyFunc<void(ResultCode, nx::String /*sessionId*/)> handler);
    /**
     * After successful call socket provided represents connection to the requested peer.
     */
    void openConnectionToTheTargetHost(
        nx::String& sessionId,
        nx::utils::MoveOnlyFunc<void(ResultCode, std::unique_ptr<AbstractStreamSocket>)> handler);

    SystemError::ErrorCode prevRequestSysErrorCode() const;

private:
    const SocketAddress m_relayEndpoint;

    virtual void stopWhileInAioThread() override;
};

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
