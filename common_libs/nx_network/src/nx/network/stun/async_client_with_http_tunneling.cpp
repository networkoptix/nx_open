#include "async_client_with_http_tunneling.h"

#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
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
    m_reconnectTimer.reset();

    connectInternal(lock, std::move(handler));
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
    QnMutexLocker lock(&m_mutex);
    m_reconnectHandlers.emplace(client, std::move(handler));
}

void AsyncClientWithHttpTunneling::sendRequest(
    Message request,
    RequestHandler handler,
    void* client)
{
    QnMutexLocker lock(&m_mutex);

    invokeOrPostpone(
        [request = std::move(request), handler = std::move(handler), client](
            AbstractAsyncClient* clientPtr) mutable
        {
            clientPtr->sendRequest(std::move(request), std::move(handler), client);
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

void AsyncClientWithHttpTunneling::closeConnection(SystemError::ErrorCode errorCode)
{
    using namespace std::placeholders; 

    QnMutexLocker lock(&m_mutex);
    invokeOrPostpone(
        std::bind(&AbstractAsyncClient::closeConnection, _1, errorCode));
}

void AsyncClientWithHttpTunneling::cancelHandlers(
    void* client,
    utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, client, handler = std::move(handler)]() mutable
        {
            QnMutexLocker lock(&m_mutex);

            const auto indicationHandlersSizeBak = m_indicationHandlers.size();
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
    auto onConnected = 
        [this, handler = std::move(handler)](SystemError::ErrorCode sysErrorCode)
        {
            if (sysErrorCode == SystemError::noError)
                applyConnectionSettings();
            handler(sysErrorCode);
        };

    if (m_url.scheme() == "http" || m_url.scheme() == "https")
    {
        openHttpTunnel(lock, m_url, std::move(onConnected));
    }
    else if (m_url.scheme() == "stun" || m_url.scheme() == "stuns")
    {
        createStunClient(lock, nullptr);
        m_stunClient->connect(m_url, std::move(onConnected));
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

    auto settings = m_settings;
    settings.reconnectPolicy.maxRetryCount = 0;
    m_stunClient = std::make_unique<AsyncClient>(
        std::move(connection),
        std::move(settings));
    m_stunClient->bindToAioThread(getAioThread());
    m_stunClient->setOnConnectionClosedHandler(
        std::bind(&AsyncClientWithHttpTunneling::onConnectionClosed, this, _1));
    m_stunClient->setIndicationHandler(
        nx::stun::kEveryIndicationMethod,
        std::bind(&AsyncClientWithHttpTunneling::dispatchIndication, this, _1),
        this);
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
    m_connectHandler = std::move(handler);
    m_httpClient = std::make_unique<nx_http::AsyncClient>();
    m_httpClient->bindToAioThread(getAioThread());
    m_httpClient->doUpgrade(
        url,
        nx::stun::StunOverHttpServer::kStunProtocolName,
        std::bind(&AsyncClientWithHttpTunneling::onHttpConnectionUpgradeDone, this));
}

void AsyncClientWithHttpTunneling::onHttpConnectionUpgradeDone()
{
    std::unique_ptr<nx_http::AsyncClient> httpClient;
    httpClient.swap(m_httpClient);

    if (httpClient->failed())
    {
        nx::utils::swapAndCall(m_connectHandler, httpClient->lastSysErrorCode());
        return;
    }

    if (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::switchingProtocols)
    {
        nx::utils::swapAndCall(m_connectHandler, SystemError::connectionRefused);
        return;
    }

    {
        QnMutexLocker lock(&m_mutex);

        createStunClient(lock, httpClient->takeSocket());
        makeCachedStunClientCalls();
    }

    nx::utils::swapAndCall(m_connectHandler, SystemError::noError);
}

void AsyncClientWithHttpTunneling::makeCachedStunClientCalls()
{
    for (auto& cachedCall: m_cachedStunClientCalls)
        cachedCall(m_stunClient.get());
}

void AsyncClientWithHttpTunneling::invokeOrPostpone(
    nx::utils::MoveOnlyFunc<void(AbstractAsyncClient*)> func)
{
    if (m_stunClient)
    {
        func(m_stunClient.get());
        return;
    }

    m_cachedStunClientCalls.push_back(std::move(func));
}

void AsyncClientWithHttpTunneling::onConnectionClosed(
    SystemError::ErrorCode closeReason)
{
    NX_DEBUG(this, lm("Connection to %1 has been broken").arg(m_url));

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
        return scheduleReconnect();
    }

    NX_DEBUG(this, lm("Reconnected to %1").arg(m_url));
    for (const auto& handlerContext: m_reconnectHandlers)
    {
        nx::utils::ObjectDestructionFlag::Watcher objectDestructionWatcher(&m_destructionFlag);
        handlerContext.second();
        if (objectDestructionWatcher.objectDestroyed())
            return;
    }
}

} // namespace stun
} // namespace nx
