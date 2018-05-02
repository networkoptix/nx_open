#include "http_stream_socket_server.h"

namespace nx {
namespace network {
namespace http {

void HttpStreamSocketServer::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

std::shared_ptr<HttpServerConnection> HttpStreamSocketServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<HttpServerConnection>(
        this,
        std::move(_socket),
        m_authenticationManager,
        m_httpMessageDispatcher);
    result->setPersistentConnectionEnabled(m_persistentConnectionEnabled);
    return result;
}

} // namespace nx
} // namespace network
} // namespace http
