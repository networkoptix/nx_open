#include "http_server_builder.h"

#include "rest/http_server_rest_message_dispatcher.h"

namespace nx::network::http::server {

std::unique_ptr<HttpServer> Builder::build(
    const Settings& settings,
    AbstractAuthenticationManager* authenticator,
    rest::MessageDispatcher* messageDispatcher)
{
    if (settings.endpoints.empty() && settings.sslEndpoints.empty())
        throw std::runtime_error("No HTTP endpoint to listen");

    std::unique_ptr<HttpServer> server;
    if (!settings.endpoints.empty())
    {
        server = buildServer(
            settings, false, settings.endpoints, authenticator, messageDispatcher);
    }

    if (!settings.sslEndpoints.empty())
    {
        auto secureServer = buildServer(
            settings, true, settings.sslEndpoints, authenticator, messageDispatcher);

        if (server)
            server->append(std::move(*secureServer));
        else
            server = std::move(secureServer);
    }

    return server;
}

std::unique_ptr<HttpServer> Builder::buildServer(
    const Settings& settings,
    bool sslRequired,
    const std::vector<SocketAddress>& endpoints,
    AbstractAuthenticationManager* authenticator,
    rest::MessageDispatcher* httpMessageDispatcher)
{
    auto server = std::make_unique<HttpServer>(
        authenticator,
        httpMessageDispatcher,
        sslRequired,
        nx::network::NatTraversalSupport::disabled);

    applySettings(settings, endpoints, server.get());
    return server;
}

void Builder::applySettings(
    const Settings& settings,
    const std::vector<SocketAddress>& endpoints,
    HttpServer* httpServer)
{
    if (!httpServer->bind(endpoints))
    {
        throw std::system_error(
            SystemError::getLastOSErrorCode(),
            std::system_category());
    }

    if (settings.connectionInactivityPeriod > std::chrono::milliseconds::zero())
    {
        httpServer->forEachListener(
            [&settings](nx::network::http::HttpStreamSocketServer* server)
            {
                server->setConnectionInactivityTimeout(
                    settings.connectionInactivityPeriod);
            });
    }

    httpServer->setTcpBackLogSize(settings.tcpBacklogSize);
}

} // namespace nx::network::http::server
