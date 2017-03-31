#include "relay_outgoing_tunnel_connection.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

OutgoingTunnelConnection::OutgoingTunnelConnection(
    SocketAddress relayEndpoint,
    nx::String relaySessionId,
    std::unique_ptr<api::ClientToRelayConnection> clientToRelayConnection)
    :
    m_relayEndpoint(std::move(relayEndpoint)),
    m_relaySessionId(std::move(relaySessionId)),
    m_controlConnection(std::move(clientToRelayConnection))
{
    bindToAioThread(getAioThread());
}

OutgoingTunnelConnection::~OutgoingTunnelConnection()
{
    if (isInSelfAioThread())
        stopWhileInAioThread();
}

void OutgoingTunnelConnection::stopWhileInAioThread()
{
    m_controlConnection.reset();
    m_activeRequests.clear();
}

void OutgoingTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_controlConnection)
        m_controlConnection->bindToAioThread(aioThread);
    for (auto& request: m_activeRequests)
    {
        request->relayClient->bindToAioThread(aioThread);
        request->timer.bindToAioThread(aioThread);
    }
}

void OutgoingTunnelConnection::start()
{
}

void OutgoingTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes /*socketAttributes*/,
    OnNewConnectionHandler handler)
{
    using namespace std::placeholders;

    post(
        [this, timeout, handler = std::move(handler)]() mutable
        {
            m_activeRequests.push_back(std::make_unique<RequestContext>());
            m_activeRequests.back()->completionHandler = std::move(handler);
            m_activeRequests.back()->timer.bindToAioThread(getAioThread());
            auto requestIter = --m_activeRequests.end();
        
            auto relayClient = api::ClientToRelayConnectionFactory::create(m_relayEndpoint);
            relayClient->bindToAioThread(getAioThread());
            relayClient->openConnectionToTheTargetHost(
                m_relaySessionId,
                std::bind(&OutgoingTunnelConnection::onConnectionOpened, this,
                    _1, _2, requestIter));
            
            if (timeout > std::chrono::milliseconds::zero())
            {
                m_activeRequests.back()->timer.start(
                    timeout,
                    std::bind(&OutgoingTunnelConnection::onConnectionOpened, this,
                        api::ResultCode::timedOut, nullptr, requestIter));
            }

            m_activeRequests.back()->relayClient = std::move(relayClient);
        });
}

void OutgoingTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_tunnelClosedHandler = std::move(handler);
}

static SystemError::ErrorCode toSystemError(api::ResultCode resultCode)
{
    switch (resultCode)
    {
        case api::ResultCode::ok:
            return SystemError::noError;

        case api::ResultCode::timedOut:
            return SystemError::timedOut;

        case api::ResultCode::notFound:
            return SystemError::hostNotFound;

        default:
            return SystemError::connectionReset;
    }
}

void OutgoingTunnelConnection::onConnectionOpened(
    api::ResultCode resultCode,
    std::unique_ptr<AbstractStreamSocket> connection,
    std::list<std::unique_ptr<RequestContext>>::iterator requestIter)
{
    auto completionHandler = std::move(requestIter->get()->completionHandler);
    m_activeRequests.erase(requestIter);

    const auto errorCodeToReport = toSystemError(resultCode);

    utils::ObjectDestructionFlag::Watcher watcher(&m_objectDestructionFlag);
    completionHandler(
        errorCodeToReport,
        std::move(connection),
        resultCode == api::ResultCode::ok);
    if (watcher.objectDestroyed())
        return;

    if (resultCode != api::ResultCode::ok)
        reportTunnelClosure(errorCodeToReport);
}

void OutgoingTunnelConnection::reportTunnelClosure(SystemError::ErrorCode reason)
{
    if (!m_tunnelClosedHandler)
        return;

    decltype(m_tunnelClosedHandler) tunnelClosedHandler;
    tunnelClosedHandler.swap(m_tunnelClosedHandler);
    tunnelClosedHandler(reason);
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
