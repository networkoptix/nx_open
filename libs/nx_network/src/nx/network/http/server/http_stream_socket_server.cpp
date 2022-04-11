// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_stream_socket_server.h"

namespace nx::network::http {

HttpStreamSocketServer::~HttpStreamSocketServer()
{
    pleaseStopSync();
    closeAllConnections();
}

void HttpStreamSocketServer::setPersistentConnectionEnabled(bool value)
{
    m_persistentConnectionEnabled = value;
}

void HttpStreamSocketServer::redirectAllRequestsTo(SocketAddress addressToRedirect)
{
    m_addressToRedirect = std::move(addressToRedirect);
}

server::HttpStatistics HttpStreamSocketServer::httpStatistics() const
{
    server::HttpStatistics httpStats;
    httpStats.operator=(statistics());
    NX_MUTEX_LOCKER lock(&m_mutex);
    httpStats.operator=(m_statsCalculator.requestStatistics());
    httpStats.notFound404 = m_httpMessageDispatcher->dispatchFailures();
    httpStats.requests = m_httpMessageDispatcher->requestPathStatistics();
    return httpStats;
}

std::shared_ptr<HttpServerConnection> HttpStreamSocketServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<HttpServerConnection>(
        std::move(_socket),
        m_authenticationManager,
        m_httpMessageDispatcher,
        m_addressToRedirect);
    result->setPersistentConnectionEnabled(m_persistentConnectionEnabled);
    result->setOnResponseSent(
        [this](const auto& requestProcessingTime)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_statsCalculator.processedRequest(requestProcessingTime);
        });
    return result;
}

} // namespace nx::network::http
