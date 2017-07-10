#include "async_client_with_http_tunneling.h"

#include <nx/network/url/url_parse_helper.h>

namespace nx {
namespace stun {

void AsyncClientWithHttpTunneling::connect(
    SocketAddress endpoint,
    bool useSsl,
    ConnectHandler handler)
{
    m_stunClient.connect(std::move(endpoint), useSsl, std::move(handler));
}

bool AsyncClientWithHttpTunneling::setIndicationHandler(
    int method,
    IndicationHandler handler,
    void* client)
{
    return m_stunClient.setIndicationHandler(method, std::move(handler), client);
}

void AsyncClientWithHttpTunneling::addOnReconnectedHandler(
    ReconnectHandler handler,
    void* client)
{
    m_stunClient.addOnReconnectedHandler(std::move(handler), client);
}

void AsyncClientWithHttpTunneling::sendRequest(
    Message request,
    RequestHandler handler,
    void* client)
{
    m_stunClient.sendRequest(std::move(request), std::move(handler), client);
}

bool AsyncClientWithHttpTunneling::addConnectionTimer(
    std::chrono::milliseconds period,
    TimerHandler handler,
    void* client)
{
    return m_stunClient.addConnectionTimer(period, std::move(handler), client);
}

SocketAddress AsyncClientWithHttpTunneling::localAddress() const
{
    return m_stunClient.localAddress();
}

SocketAddress AsyncClientWithHttpTunneling::remoteAddress() const
{
    return m_stunClient.remoteAddress();
}

void AsyncClientWithHttpTunneling::closeConnection(SystemError::ErrorCode errorCode)
{
    m_stunClient.closeConnection(errorCode);
}

void AsyncClientWithHttpTunneling::cancelHandlers(
    void* client,
    utils::MoveOnlyFunc<void()> handler)
{
    m_stunClient.cancelHandlers(client, std::move(handler));
}

void AsyncClientWithHttpTunneling::setKeepAliveOptions(KeepAliveOptions options)
{
    m_stunClient.setKeepAliveOptions(options);
}

void AsyncClientWithHttpTunneling::connect(QUrl url, ConnectHandler handler)
{
    m_stunClient.connect(
        network::url::getEndpoint(url),
        url.scheme() == "stuns" || url.scheme() == "https",
        std::move(handler));
}

} // namespase stun
} // namespase nx
