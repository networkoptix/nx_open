#pragma once

#include <list>
#include <memory>

#include <boost/optional.hpp>

#include <nx/network/aio/timer.h>
#include <nx/network/socket_delegate.h>
#include <nx/utils/object_destruction_flag.h>
#include <nx/utils/thread/mutex.h>

#include "../abstract_outgoing_tunnel_connection.h"
#include "api/relay_api_client.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

class NX_NETWORK_API OutgoingTunnelConnection:
    public AbstractOutgoingTunnelConnection
{
    using base_type = AbstractOutgoingTunnelConnection;

public:
    OutgoingTunnelConnection(utils::Url relayUrl,
        nx::String relaySessionId,
        std::unique_ptr<nx::cloud::relay::api::Client> relayApiClient);

    virtual void stopWhileInAioThread() override;
    virtual void bindToAioThread(aio::AbstractAioThread* aioThread) override;

    virtual void start() override;

    virtual void establishNewConnection(
        std::chrono::milliseconds timeout,
        SocketAttributes socketAttributes,
        OnNewConnectionHandler handler) override;
    virtual void setControlConnectionClosedHandler(
        nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler) override;

    void setInactivityTimeout(std::chrono::milliseconds timeout);

private:
    struct RequestContext
    {
        std::unique_ptr<nx::cloud::relay::api::Client> relayClient;
        SocketAttributes socketAttributes;
        OnNewConnectionHandler completionHandler;
        nx::network::aio::Timer timer;
    };

    const nx::utils::Url m_relayUrl;
    const nx::String m_relaySessionId;
    std::unique_ptr<nx::cloud::relay::api::Client> m_relayApiClient;
    QnMutex m_mutex;
    std::list<std::unique_ptr<RequestContext>> m_activeRequests;
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> m_tunnelClosedHandler;
    utils::ObjectDestructionFlag m_objectDestructionFlag;
    boost::optional<std::chrono::milliseconds> m_inactivityTimeout;
    nx::network::aio::Timer m_inactivityTimer;
    std::shared_ptr<int> m_usageCounter;

    void startInactivityTimer();
    void stopInactivityTimer();
    void onConnectionOpened(
        nx::cloud::relay::api::ResultCode resultCode,
        std::unique_ptr<AbstractStreamSocket> connection,
        std::list<std::unique_ptr<RequestContext>>::iterator requestIter);
    void reportTunnelClosure(SystemError::ErrorCode reason);
    void onInactivityTimeout();
};

class OutgoingConnection:
    public StreamSocketDelegate
{
public:
    OutgoingConnection(
        std::unique_ptr<AbstractStreamSocket> delegatee,
        std::shared_ptr<int> usageCounter);
    virtual ~OutgoingConnection() override;

private:
    std::unique_ptr<AbstractStreamSocket> m_delegatee;
    std::shared_ptr<int> m_usageCounter;
};

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
