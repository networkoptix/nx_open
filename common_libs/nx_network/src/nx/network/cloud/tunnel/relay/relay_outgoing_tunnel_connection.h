#pragma once

#include <list>

#include <nx/network/aio/timer.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/thread/mutex.h>

#include "../abstract_outgoing_tunnel_connection.h"
#include "api/client_to_relay_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API OutgoingTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
    using base_type = AbstractOutgoingTunnelConnection;

public:
    OutgoingTunnelConnection(
        SocketAddress relayEndpoint,
        nx::String relaySessionId,
        std::unique_ptr<api::ClientToRelayConnection> clientToRelayConnection);
    virtual ~OutgoingTunnelConnection();

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;
    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

private:
    struct RequestContext
    {
        std::unique_ptr<api::ClientToRelayConnection> relayClient;
        OnNewConnectionHandler completionHandler;
        nx::network::aio::Timer timer;
    };

    const SocketAddress m_relayEndpoint;
    const nx::String m_relaySessionId;
    std::unique_ptr<api::ClientToRelayConnection> m_controlConnection;
    QnMutex m_mutex;
    std::list<std::unique_ptr<RequestContext>> m_activeRequests;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_tunnelClosedHandler;
    utils::ObjectDestructionFlag m_objectDestructionFlag;

    void onConnectionOpened(
        api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection,
        std::list<std::unique_ptr<RequestContext>>::iterator requestIter);
    void reportTunnelClosure(SystemError::ErrorCode reason);
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
