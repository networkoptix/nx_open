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

void HttpStreamSocketServer::setExtraResponseHeaders(HttpHeaders responseHeaders)
{
    m_extraResponseHeaders = std::move(responseHeaders);
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

    // TODO: #akolesnikov Refactor fetching statistics from the dispatcher.
    for (AbstractRequestHandler* handler = m_requestHandler; handler != nullptr;)
    {
        if (auto dispatcher = dynamic_cast<AbstractMessageDispatcher*>(handler); dispatcher)
        {
            httpStats.notFound404 = dispatcher->dispatchFailures();
            httpStats.requests = dispatcher->requestPathStatistics();
            break;
        }

        if (auto intermediary = dynamic_cast<IntermediaryHandler*>(handler); intermediary)
            handler = intermediary->nextHandler();
        else
            break;
    }

    return httpStats;
}

std::shared_ptr<HttpServerConnection> HttpStreamSocketServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<HttpServerConnection>(
        std::move(_socket),
        m_requestHandler,
        m_addressToRedirect);
    result->setPersistentConnectionEnabled(m_persistentConnectionEnabled);
    result->setExtraResponseHeaders(m_extraResponseHeaders);
    result->setOnResponseSent(
        [this](const auto& requestProcessingTime)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_statsCalculator.processedRequest(requestProcessingTime);
        });
    return result;
}

} // namespace nx::network::http
