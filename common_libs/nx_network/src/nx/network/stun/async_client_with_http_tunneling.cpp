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

void AsyncClientWithHttpTunneling::connect(
    SocketAddress endpoint,
    bool useSsl,
    ConnectHandler handler)
{
    m_stunClient = std::make_unique<AsyncClient>(m_settings);
    m_stunClient->bindToAioThread(getAioThread());
    m_stunClient->connect(std::move(endpoint), useSsl, std::move(handler));
}

bool AsyncClientWithHttpTunneling::setIndicationHandler(
    int method,
    IndicationHandler handler,
    void* client)
{
    if (m_stunClient)
        return m_stunClient->setIndicationHandler(method, std::move(handler), client);

    m_cachedStunClientCalls.push_back(
        std::bind(&AbstractAsyncClient::setIndicationHandler, std::placeholders::_1,
            method, std::move(handler), client));
    return true;
}

void AsyncClientWithHttpTunneling::addOnReconnectedHandler(
    ReconnectHandler handler,
    void* client)
{
    if (m_stunClient)
        return m_stunClient->addOnReconnectedHandler(std::move(handler), client);

    m_cachedStunClientCalls.push_back(
        std::bind(&AbstractAsyncClient::addOnReconnectedHandler, std::placeholders::_1,
            std::move(handler), client));
}

void AsyncClientWithHttpTunneling::sendRequest(
    Message request,
    RequestHandler handler,
    void* client)
{
    if (m_stunClient)
        return m_stunClient->sendRequest(std::move(request), std::move(handler), client);

    m_cachedStunClientCalls.push_back(
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
    if (m_stunClient)
        return m_stunClient->addConnectionTimer(period, std::move(handler), client);

    m_cachedStunClientCalls.push_back(
        std::bind(&AbstractAsyncClient::addConnectionTimer, std::placeholders::_1,
            period, std::move(handler), client));
    return true;
}

SocketAddress AsyncClientWithHttpTunneling::localAddress() const
{
    if (!m_stunClient)
        return SocketAddress();
    return m_stunClient->localAddress();
}

SocketAddress AsyncClientWithHttpTunneling::remoteAddress() const
{
    if (!m_stunClient)
        return SocketAddress();
    return m_stunClient->remoteAddress();
}

void AsyncClientWithHttpTunneling::closeConnection(SystemError::ErrorCode errorCode)
{
    if (m_stunClient)
        return m_stunClient->closeConnection(errorCode);

    m_cachedStunClientCalls.push_back(
        std::bind(&AbstractAsyncClient::closeConnection, std::placeholders::_1,
            errorCode));
}

void AsyncClientWithHttpTunneling::cancelHandlers(
    void* client,
    utils::MoveOnlyFunc<void()> handler)
{
    if (m_stunClient)
        return m_stunClient->cancelHandlers(client, std::move(handler));

    m_cachedStunClientCalls.push_back(
        [client, handler = std::move(handler)](
            AbstractAsyncClient* clientPtr) mutable
        {
            clientPtr->cancelHandlers(client, std::move(handler));
        });
}

void AsyncClientWithHttpTunneling::setKeepAliveOptions(KeepAliveOptions options)
{
    if (m_stunClient)
        return m_stunClient->setKeepAliveOptions(options);

    m_cachedStunClientCalls.push_back(
        std::bind(&AbstractAsyncClient::setKeepAliveOptions, std::placeholders::_1,
            std::move(options)));
}

void AsyncClientWithHttpTunneling::connect(const QUrl& url, ConnectHandler handler)
{
    if (url.scheme() == "http" || url.scheme() == "https")
    {
       openHttpTunnel(url, std::move(handler));
    }
    else
    {
        connect(
            network::url::getEndpoint(url),
            url.scheme() == "stuns" || url.scheme() == "https",
            std::move(handler));
    }
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

    m_stunClient = std::make_unique<AsyncClient>(httpClient->takeSocket());
    makeCachedStunClientCalls();
    nx::utils::swapAndCall(m_connectHandler, SystemError::noError);
}

void AsyncClientWithHttpTunneling::makeCachedStunClientCalls()
{
    for (auto& cachedCall: m_cachedStunClientCalls)
        cachedCall(m_stunClient.get());
}

} // namespace stun
} // namespace nx
