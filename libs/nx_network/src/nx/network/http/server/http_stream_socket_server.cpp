#include "http_stream_socket_server.h"

namespace nx {
namespace network {
namespace http {

HttpStreamSocketServer::~HttpStreamSocketServer()
{
    pleaseStopSync();
    closeAllConnections();
}

void HttpStreamSocketServer::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

server::HttpStatistics HttpStreamSocketServer::httpStatistics() const
{
    server::HttpStatistics httpStats;
    httpStats.add(statistics());
    QnMutexLocker lock(&m_mutex);
    httpStats.assign(m_statsCalculator.requestStatistics());
    return httpStats;
}

std::shared_ptr<HttpServerConnection> HttpStreamSocketServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<HttpServerConnection>(
        std::move(_socket),
        m_authenticationManager,
        m_httpMessageDispatcher);
    result->setPersistentConnectionEnabled(m_persistentConnectionEnabled);
    result->setOnResponseSent(
        [this](const auto& requestProcessingTime)
        {
            QnMutexLocker lock(&m_mutex);
            m_statsCalculator.add(requestProcessingTime);
        });
    return result;
}

} // namespace nx
} // namespace network
} // namespace http
