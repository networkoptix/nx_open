#include "relay_outgoing_tunnel_connection.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace cloud {
namespace relay {

OutgoingTunnelConnection::OutgoingTunnelConnection(
    nx::utils::Url relayUrl,
    nx::String relaySessionId,
    std::unique_ptr<nx::cloud::relay::api::Client> relayApiClient)
    :
    m_relayUrl(std::move(relayUrl)),
    m_relaySessionId(std::move(relaySessionId)),
    m_relayApiClient(std::move(relayApiClient)),
    m_usageCounter(std::make_shared<int>(0))
{
    bindToAioThread(getAioThread());
}

void OutgoingTunnelConnection::stopWhileInAioThread()
{
    m_inactivityTimer.pleaseStopSync();
    m_relayApiClient.reset();
    m_activeRequests.clear();
}

void OutgoingTunnelConnection::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_inactivityTimer.bindToAioThread(aioThread);
    if (m_relayApiClient)
        m_relayApiClient->bindToAioThread(aioThread);
    for (auto& request: m_activeRequests)
    {
        request->relayClient->bindToAioThread(aioThread);
        request->timer.bindToAioThread(aioThread);
    }
}

void OutgoingTunnelConnection::start()
{
    startInactivityTimer();
}

void OutgoingTunnelConnection::establishNewConnection(
    std::chrono::milliseconds timeout,
    SocketAttributes socketAttributes,
    OnNewConnectionHandler handler)
{
    using namespace std::placeholders;

    dispatch(
        [this, timeout, socketAttributes = std::move(socketAttributes),
            handler = std::move(handler)]() mutable
        {
            stopInactivityTimer();

            m_activeRequests.push_back(std::make_unique<RequestContext>());
            m_activeRequests.back()->socketAttributes = std::move(socketAttributes);
            m_activeRequests.back()->completionHandler = std::move(handler);
            m_activeRequests.back()->timer.bindToAioThread(getAioThread());
            auto requestIter = --m_activeRequests.end();
        
            std::unique_ptr<nx::cloud::relay::api::Client> relayClient;
            if (m_relayApiClient)
            {
                m_relayApiClient.swap(relayClient);
                if (relayClient->url() != m_relayUrl)
                    relayClient.reset();
            }
            if (!relayClient)
                relayClient = nx::cloud::relay::api::ClientFactory::create(m_relayUrl);

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
                        nx::cloud::relay::api::ResultCode::timedOut, nullptr, requestIter));
            }

            m_activeRequests.back()->relayClient = std::move(relayClient);
        });
}

void OutgoingTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_tunnelClosedHandler = std::move(handler);
}

std::string OutgoingTunnelConnection::toString() const
{
    return lm("Relaying. Relay server in use: %1").args(m_relayUrl).toStdString();
}

void OutgoingTunnelConnection::setInactivityTimeout(std::chrono::milliseconds timeout)
{
    m_inactivityTimeout = timeout;
}

void OutgoingTunnelConnection::startInactivityTimer()
{
    if (m_inactivityTimeout)
    {
        m_inactivityTimer.start(
            *m_inactivityTimeout,
            std::bind(&OutgoingTunnelConnection::onInactivityTimeout, this));
    }
}

void OutgoingTunnelConnection::stopInactivityTimer()
{
    m_inactivityTimer.cancelSync();
}

void OutgoingTunnelConnection::onConnectionOpened(
    nx::cloud::relay::api::ResultCode resultCode,
    std::unique_ptr<AbstractStreamSocket> connection,
    std::list<std::unique_ptr<RequestContext>>::iterator requestIter)
{
    const auto requestContext = std::move(*requestIter);
    m_activeRequests.erase(requestIter);

    const auto errorCodeToReport = toSystemError(resultCode);
    if (resultCode != nx::cloud::relay::api::ResultCode::ok)
        connection.reset(); //< Connection can be present in case of logic error.

    if (connection)
    {
        NX_ASSERT(connection->isInSelfAioThread());
        connection->cancelIOSync(aio::etNone);
        requestContext->socketAttributes.applyTo(connection.get());

        std::unique_ptr<AbstractStreamSocket> outgoingConnection =
            std::make_unique<OutgoingConnection>(
                std::move(connection),
                m_usageCounter);
        connection.swap(outgoingConnection);
    }

    utils::ObjectDestructionFlag::Watcher watcher(&m_objectDestructionFlag);
    requestContext->completionHandler(
        errorCodeToReport,
        std::move(connection),
        resultCode == nx::cloud::relay::api::ResultCode::ok);
    if (watcher.objectDestroyed())
        return;

    if (resultCode != nx::cloud::relay::api::ResultCode::ok)
        return reportTunnelClosure(errorCodeToReport);

    if (m_activeRequests.empty())
        startInactivityTimer();
}

void OutgoingTunnelConnection::reportTunnelClosure(SystemError::ErrorCode reason)
{
    if (!m_tunnelClosedHandler)
        return;

    decltype(m_tunnelClosedHandler) tunnelClosedHandler;
    tunnelClosedHandler.swap(m_tunnelClosedHandler);
    tunnelClosedHandler(reason);
}

void OutgoingTunnelConnection::onInactivityTimeout()
{
    if (m_usageCounter.unique())
    {
        m_inactivityTimer.cancelSync();
        return reportTunnelClosure(SystemError::timedOut);
    }

    m_inactivityTimer.start(
        *m_inactivityTimeout,
        std::bind(&OutgoingTunnelConnection::onInactivityTimeout, this));
}

//-------------------------------------------------------------------------------------------------
// OutgoingConnection

OutgoingConnection::OutgoingConnection(
    std::unique_ptr<AbstractStreamSocket> delegatee,
    std::shared_ptr<int> usageCounter)
    :
    StreamSocketDelegate(delegatee.get()),
    m_delegatee(std::move(delegatee)),
    m_usageCounter(std::move(usageCounter))
{
    ++(*m_usageCounter);
}

OutgoingConnection::~OutgoingConnection()
{
    --(*m_usageCounter);
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
