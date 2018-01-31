#include "async_client_with_http_tunneling.h"

#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace stun {

AsyncClientWithHttpTunneling::AsyncClientWithHttpTunneling(Settings settings):
    m_settings(settings),
    m_reconnectTimer(m_settings.reconnectPolicy)
{
    bindToAioThread(getAioThread());
}

void AsyncClientWithHttpTunneling::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_reconnectTimer.bindToAioThread(aioThread);
    if (m_stunClient)
        m_stunClient->bindToAioThread(aioThread);
    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
}

void AsyncClientWithHttpTunneling::connect(const QUrl& url, ConnectHandler handler)
{
    QnMutexLocker lock(&m_mutex);
    m_url = url;

    post(
        [this, handler = std::move(handler)]() mutable
        {
            QnMutexLocker lock(&m_mutex);

            m_reconnectTimer.reset();

            connectInternal(
                lock,
                [this, handler = std::move(handler)](
                    SystemError::ErrorCode systemErrorCode)
                {
                    if (systemErrorCode == SystemError::noError)
                        reportReconnect();
                    else
                        scheduleReconnect();
                    //< TODO: ak reportReconnect() call looks strange here.
                    // But, that is how stun::AsyncClient works and some code relies on that.
                    handler(systemErrorCode);
                });
        });
}

bool AsyncClientWithHttpTunneling::setIndicationHandler(
    int method,
    IndicationHandler handler,
    void* client)
{
    QnMutexLocker lock(&m_mutex);

    return m_indicationHandlers.emplace(
        method,
        HandlerContext{std::move(handler), client}).second;
}

void AsyncClientWithHttpTunneling::addOnReconnectedHandler(
    ReconnectHandler handler,
    void* client)
{
    post(
        [this, client, handler = std::move(handler)]() mutable
        {
            m_reconnectHandlers.emplace(client, std::move(handler));
        });
}

void AsyncClientWithHttpTunneling::setOnConnectionClosedHandler(
    OnConnectionClosedHandler handler)
{
    post(
        [this, handler = std::move(handler)]() mutable
        {
            m_connectionClosedHandler.swap(handler);
        });
}

void AsyncClientWithHttpTunneling::sendRequest(
    Message request,
    RequestHandler handler,
    void* clientId)
{
    using namespace std::placeholders;

    dispatch(
        [this, request = std::move(request), handler = std::move(handler), clientId]() mutable
        {
            const auto requestId = ++m_requestIdSequence;
            RequestContext requestContext;
            requestContext.request = std::move(request);
            requestContext.handler = std::move(handler);
            requestContext.clientId = clientId;

            if (m_stunClient)
            {
                m_stunClient->sendRequest(
                    requestContext.request,
                    std::bind(&AsyncClientWithHttpTunneling::onRequestCompleted, this,
                        _1, _2, requestId));
            }

            m_activeRequests.emplace(requestId, std::move(requestContext));
        });
}

bool AsyncClientWithHttpTunneling::addConnectionTimer(
    std::chrono::milliseconds period,
    TimerHandler handler,
    void* client)
{
    QnMutexLocker lock(&m_mutex);

    if (m_stunClient)
        return m_stunClient->addConnectionTimer(period, std::move(handler), client);
    return false;
}

SocketAddress AsyncClientWithHttpTunneling::localAddress() const
{
    QnMutexLocker lock(&m_mutex);

    if (!m_stunClient)
        return SocketAddress();
    return m_stunClient->localAddress();
}

SocketAddress AsyncClientWithHttpTunneling::remoteAddress() const
{
    QnMutexLocker lock(&m_mutex);

    if (!m_stunClient)
        return SocketAddress();
    return m_stunClient->remoteAddress();
}

void AsyncClientWithHttpTunneling::closeConnection(SystemError::ErrorCode reason)
{
    dispatch(
        [this, reason]()
        {
            decltype(m_stunClient) stunClient;
            stunClient.swap(m_stunClient);
            if (stunClient)
            {
                stunClient->closeConnection(reason);
                stunClient.reset();
            }
            if (m_httpClient)
                m_httpClient.reset();

            decltype(m_activeRequests) activeRequests;
            activeRequests.swap(m_activeRequests);
            for (auto& requestContext: activeRequests)
                requestContext.second.handler(reason, nx::stun::Message());
        });
}

void AsyncClientWithHttpTunneling::cancelHandlers(
    void* client,
    utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, client, handler = std::move(handler)]() mutable
        {
            QnMutexLocker lock(&m_mutex);

            for (auto it = m_indicationHandlers.begin(); it != m_indicationHandlers.end(); )
            {
                if (it->second.client == client)
                    it = m_indicationHandlers.erase(it);
                else
                    ++it;
            }

            m_reconnectHandlers.erase(client);

            if (!m_stunClient)
                return handler();
            m_stunClient->cancelHandlers(client, std::move(handler));
        });
}

void AsyncClientWithHttpTunneling::setKeepAliveOptions(KeepAliveOptions options)
{
    QnMutexLocker lock(&m_mutex);

    m_keepAliveOptions = std::move(options);
    if (m_stunClient)
        m_stunClient->setKeepAliveOptions(*m_keepAliveOptions);
}

void AsyncClientWithHttpTunneling::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_reconnectTimer.pleaseStopSync();
    m_stunClient.reset();
    m_httpClient.reset();
}

void AsyncClientWithHttpTunneling::connectInternal(
    const QnMutexLockerBase& lock,
    ConnectHandler handler)
{
    NX_ASSERT(isInSelfAioThread());

    auto onConnected =
        [this, handler = std::move(handler)](SystemError::ErrorCode sysErrorCode)
        {
            if (sysErrorCode == SystemError::noError)
                applyConnectionSettings();
            handler(sysErrorCode);
        };

    if (m_url.scheme() == nx_http::kUrlSchemeName ||
        m_url.scheme() == nx_http::kSecureUrlSchemeName)
    {
        openHttpTunnel(lock, m_url, std::move(onConnected));
    }
    else if (m_url.scheme() == nx::stun::kUrlSchemeName ||
        m_url.scheme() == nx::stun::kSecureUrlSchemeName)
    {
        createStunClient(lock, nullptr);
        m_stunClient->connect(m_url, std::move(onConnected));
        sendPendingRequests();
    }
    else
    {
        post([onConnected = std::move(onConnected)]() { onConnected(SystemError::invalidData); });
    }
}

void AsyncClientWithHttpTunneling::applyConnectionSettings()
{
    if (m_keepAliveOptions)
        m_stunClient->setKeepAliveOptions(*m_keepAliveOptions);
}

void AsyncClientWithHttpTunneling::createStunClient(
    const QnMutexLockerBase&,
    std::unique_ptr<AbstractStreamSocket> connection)
{
    using namespace std::placeholders;

    NX_ASSERT(isInSelfAioThread());

    auto settings = m_settings;
    settings.reconnectPolicy.maxRetryCount = 0;
    m_stunClient = std::make_unique<AsyncClient>(
        std::move(connection),
        std::move(settings));
    m_stunClient->bindToAioThread(getAioThread());
    m_stunClient->setOnConnectionClosedHandler(
        std::bind(&AsyncClientWithHttpTunneling::onStunConnectionClosed, this, _1));
    m_stunClient->setIndicationHandler(
        nx::stun::kEveryIndicationMethod,
        std::bind(&AsyncClientWithHttpTunneling::dispatchIndication, this, _1),
        this);
}

void AsyncClientWithHttpTunneling::sendPendingRequests()
{
    using namespace std::placeholders;

    for (const auto& requestContext: m_activeRequests)
    {
        m_stunClient->sendRequest(
            requestContext.second.request,
            std::bind(&AsyncClientWithHttpTunneling::onRequestCompleted, this,
                _1, _2, requestContext.first));
    }
}

void AsyncClientWithHttpTunneling::dispatchIndication(
    nx::stun::Message indication)
{
    QnMutexLocker lock(&m_mutex);

    NX_VERBOSE(this, lm("Received indication %1 from %2")
        .arg(indication.header.method).arg(m_url));

    auto it = m_indicationHandlers.find(indication.header.method);
    if (it == m_indicationHandlers.end())
        it = m_indicationHandlers.find(nx::stun::kEveryIndicationMethod);

    if (it == m_indicationHandlers.end())
    {
        NX_DEBUG(this, lm("Ignoring unexpected indication %1").arg(indication.header.method));
        return;
    }

    auto handler = it->second.handler;
    lock.unlock();

    handler(std::move(indication));
}

void AsyncClientWithHttpTunneling::openHttpTunnel(
    const QnMutexLockerBase&,
    const QUrl& url,
    ConnectHandler handler)
{
    m_httpTunnelEstablishedHandler = std::move(handler);
    m_httpClient = std::make_unique<nx_http::AsyncClient>();
    m_httpClient->bindToAioThread(getAioThread());
    m_httpClient->doUpgrade(
        url,
        nx::stun::StunOverHttpServer::kStunProtocolName,
        std::bind(&AsyncClientWithHttpTunneling::onHttpConnectionUpgradeDone, this));
}

void AsyncClientWithHttpTunneling::onHttpConnectionUpgradeDone()
{
    SystemError::ErrorCode resultCode = SystemError::noError;
    auto connecttionEventsReporter = makeScopeGuard(
        [this, &resultCode]()
        {
            if (resultCode != SystemError::noError)
                closeConnection(resultCode);
            nx::utils::swapAndCall(m_httpTunnelEstablishedHandler, resultCode);
        });

    std::unique_ptr<nx_http::AsyncClient> httpClient;
    httpClient.swap(m_httpClient);

    if (httpClient->failed())
    {
        NX_DEBUG(this, lm("Connection to %1 failed with error %2").arg(m_url)
            .arg(SystemError::toString(httpClient->lastSysErrorCode())));
        resultCode = httpClient->lastSysErrorCode();
        return;
    }

    if (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::switchingProtocols)
    {
        NX_DEBUG(this, lm("Connection to %1 failed with HTTP status %2")
            .arg(m_url)
            .arg(nx_http::StatusCode::toString(httpClient->response()->statusLine.statusCode)));
        resultCode = SystemError::connectionRefused;
        return;
    }

    auto connection = httpClient->takeSocket();
    if (!connection->setRecvTimeout(std::chrono::milliseconds::zero()) ||
        !connection->setSendTimeout(std::chrono::milliseconds::zero()))
    {
        resultCode = SystemError::getLastOSErrorCode();
        NX_DEBUG(this, lm("Error changing socket timeout. %1")
            .arg(SystemError::toString(resultCode)));
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);
        createStunClient(lock, std::move(connection));
        sendPendingRequests();
    }
}

void AsyncClientWithHttpTunneling::onRequestCompleted(
    SystemError::ErrorCode sysErrorCode,
    nx::stun::Message response,
    int requestId)
{
    NX_ASSERT(isInSelfAioThread());

    auto requestIter = m_activeRequests.find(requestId);
    if (requestIter == m_activeRequests.end())
    {
        NX_ASSERT(false);
        NX_DEBUG(this, lm("Received response from %1 with unexpected request id %2")
            .arg(m_url).arg(requestId));
        return;
    }

    auto requestContext = std::move(requestIter->second);
    m_activeRequests.erase(requestIter);

    requestContext.handler(sysErrorCode, std::move(response));
}

void AsyncClientWithHttpTunneling::onStunConnectionClosed(
    SystemError::ErrorCode closeReason)
{
    NX_ASSERT(isInSelfAioThread());

    NX_DEBUG(this, lm("Connection to %1 has been broken. %2")
        .arg(m_url).arg(SystemError::toString(closeReason)));

    closeConnection(closeReason);

    if (m_connectionClosedHandler)
        m_connectionClosedHandler(closeReason);

    scheduleReconnect();
}

void AsyncClientWithHttpTunneling::scheduleReconnect()
{
    if (!m_reconnectTimer.scheduleNextTry(
            std::bind(&AsyncClientWithHttpTunneling::reconnect, this)))
    {
        NX_DEBUG(this, lm("Giving up reconnect to %1 attempts").arg(m_url));
        // TODO: #ak It makes sense to add "connection closed" event and raise it here.
    }
}

void AsyncClientWithHttpTunneling::reconnect()
{
    using namespace std::placeholders;

    QnMutexLocker lock(&m_mutex);
    connectInternal(
        lock,
        std::bind(&AsyncClientWithHttpTunneling::onReconnectDone, this, _1));
}

void AsyncClientWithHttpTunneling::onReconnectDone(SystemError::ErrorCode sysErrorCode)
{
    if (sysErrorCode != SystemError::noError)
    {
        NX_DEBUG(this, lm("Reconnect to %1 failed with result %2")
            .arg(m_url).arg(SystemError::toString(sysErrorCode)));
        closeConnection(sysErrorCode);
        return scheduleReconnect();
    }

    reportReconnect();
}

void AsyncClientWithHttpTunneling::reportReconnect()
{
    NX_DEBUG(this, lm("Reconnected to %1").arg(m_url));
    for (const auto& handlerContext: m_reconnectHandlers)
    {
        // TODO: #ak Add support for m_reconnectHandlers being modified within handler.
        nx::utils::ObjectDestructionFlag::Watcher objectDestructionWatcher(&m_destructionFlag);
        handlerContext.second();
        if (objectDestructionWatcher.objectDestroyed())
            return;
    }
}

} // namespace stun
} // namespace nx
