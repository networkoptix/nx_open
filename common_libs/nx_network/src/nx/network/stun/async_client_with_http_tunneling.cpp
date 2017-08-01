#include "async_client_with_http_tunneling.h"

#include <nx/network/stun/stun_over_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/std/cpp14.h>

namespace nx {
namespace stun {

AsyncClientWithHttpTunneling::AsyncClientWithHttpTunneling(Settings settings):
    m_settings(settings)
{
}

void AsyncClientWithHttpTunneling::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    base_type::bindToAioThread(aioThread);

    if (m_stunClient)
        m_stunClient->bindToAioThread(aioThread);
    if (m_httpClient)
        m_httpClient->bindToAioThread(aioThread);
}

void AsyncClientWithHttpTunneling::connect(const QUrl& url, ConnectHandler handler)
{
    if (url.scheme() == "http" || url.scheme() == "https")
    {
        openHttpTunnel(url, std::move(handler));
    }
    else if (url.scheme() == "stun" || url.scheme() == "stuns")
    {
        QnMutexLocker lock(&m_mutex);

        m_stunClient = std::make_unique<AsyncClient>(m_settings);
        m_stunClient->bindToAioThread(getAioThread());
        m_stunClient->connect(url, std::move(handler));
    }
}

bool AsyncClientWithHttpTunneling::setIndicationHandler(
    int method,
    IndicationHandler handler,
    void* client)
{
    QnMutexLocker lock(&m_mutex);

    if (m_stunClient)
        return m_stunClient->setIndicationHandler(method, std::move(handler), client);

    if (!m_indicationHandlers.emplace(
            method,
            HandlerContext{std::move(handler), client}).second)
    {
        return false;
    }

    return true;
}

void AsyncClientWithHttpTunneling::addOnReconnectedHandler(
    ReconnectHandler handler,
    void* client)
{
    QnMutexLocker lock(&m_mutex);

    invokeOrPostpone(
        std::bind(&AbstractAsyncClient::addOnReconnectedHandler, std::placeholders::_1,
            std::move(handler), client));
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
    QnMutexLocker lock(&m_mutex);

    invokeOrPostpone(
        std::bind(&AbstractAsyncClient::closeConnection, std::placeholders::_1, errorCode));
}

void AsyncClientWithHttpTunneling::cancelHandlers(
    void* client,
    utils::MoveOnlyFunc<void()> handler)
{
    QnMutexLocker lock(&m_mutex);

    if (m_stunClient)
        return m_stunClient->cancelHandlers(client, std::move(handler));

    const auto indicationHandlersSizeBak = m_indicationHandlers.size();
    for (auto it = m_indicationHandlers.begin(); it != m_indicationHandlers.end(); )
    {
        if (it->second.client == client)
            it = m_indicationHandlers.erase(it);
        else
            ++it;
    }
    
    if (m_indicationHandlers.size() == indicationHandlersSizeBak)
    {
        return invokeOrPostpone(
            [client, handler = std::move(handler)](
                AbstractAsyncClient* clientPtr) mutable
            {
                clientPtr->cancelHandlers(client, std::move(handler));
            });
    }

    post([handler = std::move(handler)]() { handler(); });
}

void AsyncClientWithHttpTunneling::setKeepAliveOptions(KeepAliveOptions options)
{
    QnMutexLocker lock(&m_mutex);

    invokeOrPostpone(
        std::bind(&AbstractAsyncClient::setKeepAliveOptions, 
            std::placeholders::_1, std::move(options)));
}

void AsyncClientWithHttpTunneling::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_stunClient.reset();
    m_httpClient.reset();
}

void AsyncClientWithHttpTunneling::openHttpTunnel(
    const QUrl& url,
    ConnectHandler handler)
{
    QnMutexLocker lock(&m_mutex);

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

        m_stunClient = std::make_unique<AsyncClient>(httpClient->takeSocket());
        makeCachedStunClientCalls();
    }

    nx::utils::swapAndCall(m_connectHandler, SystemError::noError);
}

void AsyncClientWithHttpTunneling::makeCachedStunClientCalls()
{
    for (auto& cachedCall: m_cachedStunClientCalls)
        cachedCall(m_stunClient.get());

    for (auto& elem: m_indicationHandlers)
    {
        m_stunClient->setIndicationHandler(
            elem.first,
            std::move(elem.second.handler),
            elem.second.client);
    }
    m_indicationHandlers.clear();
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

} // namespace stun
} // namespace nx
