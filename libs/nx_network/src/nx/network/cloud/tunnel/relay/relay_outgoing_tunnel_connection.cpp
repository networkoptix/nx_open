// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "relay_outgoing_tunnel_connection.h"

#include <nx/network/url/url_builder.h>
#include <nx/utils/log/log.h>

#include "../../protocol_type.h"

namespace nx::network::cloud::relay {

OutgoingTunnelConnection::OutgoingTunnelConnection(
    nx::utils::Url relayUrl,
    std::string relaySessionId,
    std::unique_ptr<nx::cloud::relay::api::AbstractClient> relayApiClient)
    :
    m_relayUrl(std::move(relayUrl)),
    m_relaySessionId(std::move(relaySessionId)),
    m_relayApiClient(std::move(relayApiClient)),
    m_usageCounter(std::make_shared<int>(0))
{
    bindToAioThread(getAioThread());

    NX_VERBOSE(this, "%1. Created new relay tunnel. Url %2", m_relaySessionId, m_relayUrl);
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
    dispatch(
        [this, timeout, socketAttributes = std::move(socketAttributes),
            handler = std::move(handler)]() mutable
        {
            NX_VERBOSE(this, "%1. Opening new connection with timeout %2", m_relaySessionId, timeout);

            stopInactivityTimer();

            m_activeRequests.push_back(std::make_unique<RequestContext>());
            m_activeRequests.back()->socketAttributes = std::move(socketAttributes);
            m_activeRequests.back()->completionHandler = std::move(handler);
            m_activeRequests.back()->timer.bindToAioThread(getAioThread());
            auto requestIter = --m_activeRequests.end();

            std::unique_ptr<nx::cloud::relay::api::AbstractClient> relayClient;
            if (m_relayApiClient)
            {
                m_relayApiClient.swap(relayClient);
                if (relayClient->url() != m_relayUrl)
                    relayClient.reset();
            }
            if (!relayClient)
                relayClient = std::make_unique<nx::cloud::relay::api::Client>(m_relayUrl);

            relayClient->bindToAioThread(getAioThread());
            relayClient->setTimeout(timeout);
            relayClient->openConnectionToTheTargetHost(
                m_relaySessionId,
                [this, requestIter](auto&&... args)
                {
                    onConnectionOpened(std::forward<decltype(args)>(args)..., requestIter);
                });

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
    return nx::format("Relaying. Relay server in use: %1").args(m_relayUrl).toStdString();
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
    NX_VERBOSE(this, "%1. Open connection completed with result %2", m_relaySessionId, resultCode);

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

    utils::InterruptionFlag::Watcher watcher(&m_objectDestructionFlag);
    completionHandler(
        errorCodeToReport,
        std::move(connection),
        resultCode == nx::cloud::relay::api::ResultCode::ok);
    if (watcher.interrupted())
        return;

    if (resultCode != nx::cloud::relay::api::ResultCode::ok)
        return reportTunnelClosure(errorCodeToReport);

    if (m_activeRequests.empty())
        startInactivityTimer();
}

void OutgoingTunnelConnection::reportTunnelClosure(SystemError::ErrorCode reason)
{
    NX_VERBOSE(this, "%1. Tunnel is closed with reason %2",
        m_relaySessionId, SystemError::toString(reason));

    if (m_tunnelClosedHandler)
        nx::utils::swapAndCall(m_tunnelClosedHandler, reason);
}

void OutgoingTunnelConnection::onInactivityTimeout()
{
    if (m_usageCounter.use_count() == 1)
    {
        NX_VERBOSE(this, "%1. Closing tunnel due to inactivity", m_relaySessionId);

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

} // namespace nx::network::cloud::relay
