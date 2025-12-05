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

void HttpStreamSocketServer::setExtraSuccessResponseHeaders(HttpHeaders responseHeaders)
{
    m_extraSuccessResponseHeaders = std::move(responseHeaders);
}

void HttpStreamSocketServer::redirectAllRequestsTo(SocketAddress addressToRedirect)
{
    m_addressToRedirect = std::move(addressToRedirect);
}

server::HttpStatistics HttpStreamSocketServer::httpStatistics() const
{
    server::HttpStatistics httpStatistics{statistics()};
    NX_MUTEX_LOCKER lock(&m_mutex);
    httpStatistics.operator=(m_statisticsCalculator.statistics());

    // TODO: #akolesnikov Refactor fetching statistics from the dispatcher. Currently we have to
    // check all AbstractRequestHandlers to find the dispatcher.
    for (AbstractRequestHandler* handler = m_requestHandler; handler != nullptr;)
    {
        if (auto dispatcher = dynamic_cast<AbstractMessageDispatcher*>(handler); dispatcher)
        {
            httpStatistics.requests = dispatcher->requestLineStatistics();
            break;
        }

        if (auto intermediary = dynamic_cast<IntermediaryHandler*>(handler); intermediary)
            handler = intermediary->nextHandler();
        else
            break;
    }

    return httpStatistics;
}

std::shared_ptr<HttpServerConnection> HttpStreamSocketServer::createConnection(
    std::unique_ptr<AbstractStreamSocket> _socket)
{
    auto result = std::make_shared<HttpServerConnection>(
        std::move(_socket),
        m_requestHandler,
        m_addressToRedirect);
    result->setPersistentConnectionEnabled(m_persistentConnectionEnabled);
    result->setExtraSuccessResponseHeaders(m_extraSuccessResponseHeaders);
    result->setOnResponseSent(
        [this](const auto& requestProcessingTime, auto statusCode)
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_statisticsCalculator.processedRequest(requestProcessingTime, statusCode);
        });
    return result;
}

} // namespace nx::network::http
