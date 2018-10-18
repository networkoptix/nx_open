#pragma once

#include <memory>

#include <nx/network/http/buffer_source.h>
#include <nx/network/http/test_http_server.h>

namespace nx::cctu {

class HttpServer
{
public:
    template<typename ConnectionAcceptor>
    HttpServer(std::unique_ptr<ConnectionAcceptor> connectionAcceptor);

private:
    std::unique_ptr<nx::network::http::TestHttpServer> m_httpServer;

    void processRequest(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler);
};

//-------------------------------------------------------------------------------------------------

template<typename ConnectionAcceptor>
HttpServer::HttpServer(std::unique_ptr<ConnectionAcceptor> connectionAcceptor)
{
    m_httpServer = std::make_unique<nx::network::http::TestHttpServer>(
        std::move(connectionAcceptor));
    m_httpServer->registerRequestProcessorFunc(
        nx::network::http::kAnyPath,
        [this](auto... args) { processRequest(std::move(args)...); });
    m_httpServer->server().start();
}

} // namespace nx::cctu
