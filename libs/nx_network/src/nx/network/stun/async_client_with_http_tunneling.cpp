// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "async_client_with_http_tunneling.h"

#include <nx/network/socket_global.h>
#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/stun/stun_types.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace network {
namespace stun {

static constexpr char kTunnelTag[] = "STUN over HTTP tunnel";

AsyncClientWithHttpTunneling::AsyncClientWithHttpTunneling(Settings settings):
    m_settings(settings),
    m_reconnectTimer(m_settings.reconnectPolicy)
{
    SocketGlobals::instance().allocationAnalyzer().recordObjectCreation(this);
    ++SocketGlobals::instance().debugCounters().stunOverHttpClientConnectionCount;

    bindToAioThread(getAioThread());
}

AsyncClientWithHttpTunneling::~AsyncClientWithHttpTunneling()
{
    --SocketGlobals::instance().debugCounters().stunOverHttpClientConnectionCount;
    SocketGlobals::instance().allocationAnalyzer().recordObjectDestruction(this);
}

void AsyncClientWithHttpTunneling::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    m_reconnectTimer.bindToAioThread(aioThread);
    if (m_stunClient)
        m_stunClient->bindToAioThread(aioThread);
    if (m_httpTunnelingClient)
        m_httpTunnelingClient->bindToAioThread(aioThread);
}

void AsyncClientWithHttpTunneling::connect(const nx::utils::Url& url, ConnectHandler handler)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_url = url;

    post(
        [this, handler = std::move(handler)]() mutable
        {
            NX_MUTEX_LOCKER lock(&m_mutex);

            m_reconnectTimer.reset();

            m_userConnectHandler = std::move(handler);

            connectInternal(
                lock,
                [this](SystemError::ErrorCode systemErrorCode)
                {
                    if (systemErrorCode == SystemError::noError)
                        reportReconnect();
                    else
                        scheduleReconnect();
                    //< TODO: #akolesnikov reportReconnect() call looks strange here.
                    // But, that is how stun::AsyncClient works and some code relies on that.
                });
        });
}

void AsyncClientWithHttpTunneling::setIndicationHandler(
    int method,
    IndicationHandler handler,
    void* client)
{
    NX_VERBOSE(this, "Set indication %1 handler to client %2", method, client);

    NX_MUTEX_LOCKER lock(&m_mutex);
    m_indicationHandlers[method] = HandlerContext{std::move(handler), client};
}

void AsyncClientWithHttpTunneling::addOnReconnectedHandler(
    ReconnectHandler handler,
    void* client)
{
    dispatch(
        [this, client, handler = std::move(handler)]() mutable
        {
            m_reconnectHandlers.emplace(client, std::move(handler));
        });
}

void AsyncClientWithHttpTunneling::setOnConnectionClosedHandler(
    OnConnectionClosedHandler handler)
{
    dispatch(
        [this, handler = std::move(handler)]() mutable
        {
            m_connectionClosedHandler.swap(handler);
        });
}

void AsyncClientWithHttpTunneling::sendRequest(
    Message request,
    RequestHandler handler,
    void* client)
{
    using namespace std::placeholders;

    dispatch(
        [this, request = std::move(request), handler = std::move(handler), client]() mutable
        {
            const auto requestId = ++m_requestIdSequence;
            RequestContext requestContext;
            requestContext.request = std::move(request);
            requestContext.handler = std::move(handler);
            requestContext.client = client;

            NX_VERBOSE(this, "Sending request %1 (id %2)", request.header, requestId);

            if (m_stunClient)
            {
                m_stunClient->sendRequest(
                    requestContext.request,
                    std::bind(&AsyncClientWithHttpTunneling::onRequestCompleted, this,
                        _1, _2, requestId));
            }
            else
            {
                NX_VERBOSE(this, "Request %1 (id %2) is postponed", request.header, requestId);
            }

            m_activeRequests.emplace(requestId, std::move(requestContext));
        });
}

bool AsyncClientWithHttpTunneling::addConnectionTimer(
    std::chrono::milliseconds period,
    TimerHandler handler,
    void* client)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (m_stunClient)
        return m_stunClient->addConnectionTimer(period, std::move(handler), client);
    return false;
}

SocketAddress AsyncClientWithHttpTunneling::localAddress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    if (!m_stunClient)
        return SocketAddress();
    return m_stunClient->localAddress();
}

SocketAddress AsyncClientWithHttpTunneling::remoteAddress() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);

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
            {
                NX_MUTEX_LOCKER lock(&m_mutex);
                stunClient.swap(m_stunClient);
            }

            if (stunClient)
            {
                stunClient->closeConnection(reason);
                stunClient.reset();
            }
            if (m_httpTunnelingClient)
                m_httpTunnelingClient.reset();

            decltype(m_activeRequests) activeRequests;
            activeRequests.swap(m_activeRequests);
            for (auto& requestContext: activeRequests)
                requestContext.second.handler(reason, Message());
        });
}

void AsyncClientWithHttpTunneling::cancelHandlers(
    void* client,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    post(
        [this, client, handler = std::move(handler)]() mutable
        {
            cancelHandlersSync(client);
            handler();
        });
}

void AsyncClientWithHttpTunneling::cancelHandlersSync(void* client)
{
    if (isInSelfAioThread())
    {
        NX_MUTEX_LOCKER lock(&m_mutex);

        cancelUserHandlers(m_activeRequests, client);
        cancelUserHandlers(m_indicationHandlers, client);
        m_reconnectHandlers.erase(client);

        NX_VERBOSE(this, "Removed request/indication handlers for client %1", client);

        if (m_stunClient)
            m_stunClient->cancelHandlersSync(client);
    }
    else
    {
        std::promise<void> done;
        cancelHandlers(client, [&done]() { done.set_value(); });
        done.get_future().wait();
    }
}

void AsyncClientWithHttpTunneling::setKeepAliveOptions(KeepAliveOptions options)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    m_keepAliveOptions = std::move(options);
    if (m_stunClient)
        m_stunClient->setKeepAliveOptions(*m_keepAliveOptions);
}

void AsyncClientWithHttpTunneling::setTunnelValidatorFactory(
    http::tunneling::TunnelValidatorFactoryFunc func)
{
    m_tunnelValidatorFactory = std::move(func);
}

void AsyncClientWithHttpTunneling::setHttpTunnelingClientHeaders(http::HttpHeaders headers)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_customHeaders = std::move(headers);
}

void AsyncClientWithHttpTunneling::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_reconnectTimer.pleaseStopSync();

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_stunClient.reset();
    }

    m_httpTunnelingClient.reset();
}

void AsyncClientWithHttpTunneling::connectInternal(
    const nx::Locker<nx::Mutex>& lock,
    ConnectHandler handler)
{
    NX_ASSERT(isInSelfAioThread());

    auto onConnected =
        [this, handler = std::move(handler)](SystemError::ErrorCode sysErrorCode)
        {
            if (sysErrorCode == SystemError::noError)
                applyConnectionSettings();
            handler(sysErrorCode);
            if (m_userConnectHandler)
                nx::utils::swapAndCall(m_userConnectHandler, sysErrorCode);
        };

    if (http::isUrlScheme(m_url.scheme()))
    {
        openHttpTunnel(lock, m_url, std::move(onConnected));
    }
    else if (stun::isUrlScheme(m_url.scheme()))
    {
        createStunClient(lock, nullptr);
        m_stunClient->connect(m_url, std::move(onConnected));
        sendPendingRequests();
    }
    else
    {
        post(
            [onConnected = std::move(onConnected)]()
            {
                onConnected(SystemError::invalidData);
            });
    }
}

void AsyncClientWithHttpTunneling::applyConnectionSettings()
{
    if (m_keepAliveOptions)
        m_stunClient->setKeepAliveOptions(*m_keepAliveOptions);
}

void AsyncClientWithHttpTunneling::createStunClient(
    const nx::Locker<nx::Mutex>&,
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
        kEveryIndicationMethod,
        std::bind(&AsyncClientWithHttpTunneling::dispatchIndication, this, _1),
        this);
}

void AsyncClientWithHttpTunneling::sendPendingRequests()
{
    using namespace std::placeholders;

    for (const auto& requestContext: m_activeRequests)
    {
        NX_VERBOSE(this, "Sending pending request %1 (id %2)",
            requestContext.second.request.header, requestContext.first);

        m_stunClient->sendRequest(
            requestContext.second.request,
            std::bind(&AsyncClientWithHttpTunneling::onRequestCompleted, this,
                _1, _2, requestContext.first));
    }
}

void AsyncClientWithHttpTunneling::dispatchIndication(
    nx::network::stun::Message indication)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    NX_VERBOSE(this, nx::format("Received indication %1 from %2")
        .arg(indication.header.method).arg(m_url));

    auto it = m_indicationHandlers.find(indication.header.method);
    if (it == m_indicationHandlers.end())
        it = m_indicationHandlers.find(kEveryIndicationMethod);

    if (it == m_indicationHandlers.end())
    {
        NX_DEBUG(this, "Ignoring unexpected indication %1", indication.header.method);
        return;
    }

    auto handler = it->second.handler;
    lock.unlock();

    handler(std::move(indication));
}

void AsyncClientWithHttpTunneling::openHttpTunnel(
    const nx::Locker<nx::Mutex>&,
    const nx::utils::Url& url,
    ConnectHandler handler)
{
    using namespace std::placeholders;

    m_httpTunnelEstablishedHandler = std::move(handler);
    m_httpTunnelingClient = std::make_unique<http::tunneling::Client>(url, kTunnelTag);
    if (!m_customHeaders.empty())
        m_httpTunnelingClient->setCustomHeaders(m_customHeaders);

    m_httpTunnelingClient->setTimeout(m_settings.recvTimeout);
    m_httpTunnelingClient->setTunnelValidatorFactory(
        m_tunnelValidatorFactory);
    m_httpTunnelingClient->bindToAioThread(getAioThread());
    m_httpTunnelingClient->openTunnel(
        std::bind(&AsyncClientWithHttpTunneling::onOpenHttpTunnelCompletion, this, _1));
}

void AsyncClientWithHttpTunneling::onOpenHttpTunnelCompletion(
    nx::network::http::tunneling::OpenTunnelResult tunnelResult)
{
    SystemError::ErrorCode resultCode = SystemError::noError;
    auto connecttionEventsReporter = nx::utils::makeScopeGuard(
        [this, &resultCode]()
        {
            if (resultCode != SystemError::noError)
                closeConnection(resultCode);
            nx::utils::swapAndCall(m_httpTunnelEstablishedHandler, resultCode);
        });

    auto httpTunnelingClient = std::exchange(m_httpTunnelingClient, nullptr);
    if (tunnelResult.resultCode != nx::network::http::tunneling::ResultCode::ok)
    {
        NX_DEBUG(this, nx::format("HTTP tunnel to %1 failed. %2").args(m_url, tunnelResult));
        resultCode = tunnelResult.sysError != SystemError::noError
            ? tunnelResult.sysError
            : SystemError::connectionRefused;
        return;
    }

    if (!tunnelResult.connection->setRecvTimeout(std::chrono::milliseconds::zero()) ||
        !tunnelResult.connection->setSendTimeout(std::chrono::milliseconds::zero()))
    {
        resultCode = SystemError::getLastOSErrorCode();
        NX_DEBUG(this, nx::format("Error changing socket timeout. %1")
            .args(SystemError::toString(resultCode)));
        return;
    }

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        createStunClient(lock, std::move(tunnelResult.connection));
        sendPendingRequests();
    }
}

void AsyncClientWithHttpTunneling::onRequestCompleted(
    SystemError::ErrorCode sysErrorCode,
    nx::network::stun::Message response,
    int requestId)
{
    NX_ASSERT(isInSelfAioThread());

    NX_VERBOSE(this, "Request (id %1) completed. Result %2, response %3",
        requestId, SystemError::toString(sysErrorCode), response.header);

    auto requestIter = m_activeRequests.find(requestId);
    if (requestIter == m_activeRequests.end())
    {
        // Likely, the request has been cancelled.
        NX_DEBUG(this, nx::format("Received response from %1 with unexpected request id %2")
            .arg(m_url).arg(requestId));
        return;
    }

    auto requestContext = std::move(requestIter->second);
    m_activeRequests.erase(requestIter);

    requestContext.handler(sysErrorCode, std::move(response));
}

template<typename Dictionary>
void AsyncClientWithHttpTunneling::cancelUserHandlers(
    Dictionary& dictionary,
    void* client)
{
    for (auto it = dictionary.begin(); it != dictionary.end(); )
    {
        if (it->second.client == client)
            it = dictionary.erase(it);
        else
            ++it;
    }
}

void AsyncClientWithHttpTunneling::onStunConnectionClosed(
    SystemError::ErrorCode closeReason)
{
    NX_ASSERT(isInSelfAioThread());

    NX_DEBUG(this, nx::format("Connection to %1 has been broken. %2")
        .arg(m_url).arg(SystemError::toString(closeReason)));

    closeConnection(closeReason);

    if (m_connectionClosedHandler)
        m_connectionClosedHandler(closeReason);

    scheduleReconnect();
}

void AsyncClientWithHttpTunneling::scheduleReconnect()
{
    NX_ASSERT(isInSelfAioThread());

    if (!m_reconnectTimer.scheduleNextTry(
            std::bind(&AsyncClientWithHttpTunneling::reconnect, this)))
    {
        NX_DEBUG(this, nx::format("Giving up reconnect to %1 attempts").arg(m_url));
        // TODO: #akolesnikov It makes sense to add "connection closed" event and raise it here.
    }
}

void AsyncClientWithHttpTunneling::reconnect()
{
    using namespace std::placeholders;

    NX_MUTEX_LOCKER lock(&m_mutex);
    connectInternal(
        lock,
        std::bind(&AsyncClientWithHttpTunneling::onReconnectDone, this, _1));
}

void AsyncClientWithHttpTunneling::onReconnectDone(SystemError::ErrorCode sysErrorCode)
{
    if (sysErrorCode != SystemError::noError)
    {
        NX_DEBUG(this, nx::format("Reconnect to %1 failed with result %2")
            .arg(m_url).arg(SystemError::toString(sysErrorCode)));
        closeConnection(sysErrorCode);
        return scheduleReconnect();
    }

    reportReconnect();
}

void AsyncClientWithHttpTunneling::reportReconnect()
{
    NX_DEBUG(this, nx::format("Reconnected to %1").arg(m_url));
    for (const auto& handlerContext: m_reconnectHandlers)
    {
        // TODO: #akolesnikov Add support for m_reconnectHandlers being modified within handler.
        nx::utils::InterruptionFlag::Watcher objectDestructionWatcher(&m_destructionFlag);
        handlerContext.second();
        if (objectDestructionWatcher.interrupted())
            return;
    }
}

} // namespace stun
} // namespace network
} // namespace nx
