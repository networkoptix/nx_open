#include "relay_outgoing_tunnel_connection.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "api/detail/relay_api_client_factory.h"
#include "../../protocol_type.h"

namespace nx {
namespace network {
namespace cloud {
namespace relay {

OutgoingTunnelConnection::OutgoingTunnelConnection(
    nx::utils::Url relayUrl,
    std::string relaySessionId,
    std::unique_ptr<nx::cloud::relay::api::Client> relayApiClient)
    :
    m_relayUrl(std::move(relayUrl)),
    m_relaySessionId(std::move(relaySessionId)),
    m_relayApiClient(std::move(relayApiClient)),
    m_usageCounter(std::make_shared<int>(0))
{
    bindToAioThread(getAioThread());

    NX_VERBOSE(this, "Created new relay tunnel. Url %1, session %2", m_relayUrl, m_relaySessionId);
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
            NX_VERBOSE(this, "Opening new connection with timeout %1", timeout);

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
                relayClient = nx::cloud::relay::api::ClientFactory::instance().create(m_relayUrl);

            relayClient->bindToAioThread(getAioThread());
            relayClient->setTimeout(timeout);
            relayClient->openConnectionToTheTargetHost(
                m_relaySessionId,
                std::bind(&OutgoingTunnelConnection::onConnectionOpened, this,
                    _1, _2, requestIter));

            if (timeout > std::chrono::milliseconds::zero())
            {
                // NOTE: The relayClient is supposed to report timeout itself.
                // This timer should protect from a relay client timeout error (which actually happened).
                // So, using double timeout to give the relayClient a good chance to report
                // the connection error itself.
                m_activeRequests.back()->timer.start(
                    timeout * 2,
                    [this, requestIter, timeout]()
                    {
                        NX_VERBOSE(this, "Relay session %1. Relay client failed to respond in %2",
                            m_relaySessionId, timeout * 2);

                        onConnectionOpened(
                            nx::cloud::relay::api::ResultCode::timedOut,
                            nullptr,
                            requestIter);
                    });
            }

            m_activeRequests.back()->relayClient = std::move(relayClient);
        });
}

void OutgoingTunnelConnection::setControlConnectionClosedHandler(
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
{
    m_tunnelClosedHandler = std::move(handler);
}

ConnectType OutgoingTunnelConnection::connectType() const
{
    return ConnectType::proxy;
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
    NX_VERBOSE(this, "Open connection completed with result %1", resultCode);

    auto requestContext = std::move(*requestIter);
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

    decltype(requestContext->completionHandler) completionHandler;
    completionHandler.swap(requestContext->completionHandler);
    requestContext.reset();

    utils::ObjectDestructionFlag::Watcher watcher(&m_objectDestructionFlag);
    completionHandler(
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

    NX_VERBOSE(this, "Reporting tunnel closure. %1", SystemError::toString(reason));

    decltype(m_tunnelClosedHandler) tunnelClosedHandler;
    tunnelClosedHandler.swap(m_tunnelClosedHandler);
    tunnelClosedHandler(reason);
}

void OutgoingTunnelConnection::onInactivityTimeout()
{
    if (m_usageCounter.use_count() == 1)
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
    std::unique_ptr<AbstractStreamSocket> delegate,
    std::shared_ptr<int> usageCounter)
    :
    StreamSocketDelegate(delegate.get()),
    m_delegate(std::move(delegate)),
    m_usageCounter(std::move(usageCounter))
{
    ++(*m_usageCounter);
}

OutgoingConnection::~OutgoingConnection()
{
    --(*m_usageCounter);
}

bool OutgoingConnection::getProtocol(int* protocol) const
{
    *protocol = Protocol::relay;
    return true;
}

} // namespace relay
} // namespace cloud
} // namespace network
} // namespace nx
