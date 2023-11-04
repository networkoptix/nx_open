// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "view.h"

#include <nx/network/http/http_types.h>
#include <nx/network/http/server/multi_endpoint_server.h>
#include <nx/network/http/server/http_server_builder.h>

namespace nx::network::http::server::test {

View::View(const Settings& settings):
    m_authenticationDispatcher(&m_httpMessageDispatcher),
    m_settings(settings)
{
    m_multiAddressHttpServer = nx::network::http::server::Builder::buildOrThrow(
        m_settings.getHttpSettings(), &m_authenticationDispatcher);

    m_httpMessageDispatcher.registerRequestProcessorFunc(
        nx::network::http::Method::get, kHandlerPath, [this](auto&&... args) {
            return doResponse(std::forward<decltype(args)>(args)...);
        });

    startAcceptor();
}

View::~View()
{
    stop();
}

std::vector<network::SocketAddress> View::httpEndpoints() const
{
    return m_multiAddressHttpServer->endpoints();
}

void View::doResponse(
    nx::network::http::RequestContext requestContext,
    nx::network::http::RequestProcessedHandler completionHandler)
{
    ++m_curAttemptNum;
    if (m_curAttemptNum == m_successAttemptNum)
    {
        Response response{.httpRequest = requestContext.request.toString()};

        nx::network::http::RequestResult reply(nx::network::http::StatusCode::ok);
        reply.body = std::make_unique<BufferSource>(
            "application/json",
            nx::reflect::json::serialize(response));
        return completionHandler(std::move(reply));
    }
    else
    {
        completionHandler(
            nx::network::http::RequestResult(nx::network::http::StatusCode::internalServerError));
    }
}

void View::start()
{
    if (!m_multiAddressHttpServer->listen())
    {
        throw std::system_error(
            SystemError::getLastOSErrorCode(),
            std::system_category(),
            nx::format("Cannot start listening: %1")
                .args(SystemError::getLastOSErrorText())
                .toStdString());
    }

    NX_INFO(
        this,
        "HTTP server is listening on %1",
        containerString(m_multiAddressHttpServer->endpoints()));
}

void View::stop()
{
    // Stopping accepting new connections.
    m_multiAddressHttpServer->pleaseStopSync();

    m_multiAddressHttpServer->forEachListener(
        [](const auto& listener) { listener->closeAllConnections(); });
}

void View::startAcceptor()
{
    const auto& httpEndpoints = m_settings.getHttpSettings().endpoints;
    if (httpEndpoints.empty())
    {
        NX_ERROR(this, "No HTTP address to listen");
        throw std::runtime_error("No HTTP address to listen");
    }

    if (!m_multiAddressHttpServer->bind(httpEndpoints))
    {
        throw std::runtime_error(nx::format("Cannot bind to address(es) %1. %2")
                                     .container(httpEndpoints)
                                     .args(SystemError::getLastOSErrorText())
                                     .toStdString());
    }
}

} // namespace nx::network::http::server::test
