// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_builder.h"

#include <nx/network/url/url_builder.h>

#include "https_server_context.h"

namespace nx::network::http::server {

std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> Builder::build(
    const Settings& settings,
    AbstractRequestHandler* requestHandler)
{
    if (settings.endpoints.empty() && settings.ssl.endpoints.empty())
        throw std::runtime_error("No HTTP endpoint to listen");

    std::unique_ptr<MultiEndpointServer> server;
    if (!settings.endpoints.empty())
    {
        SystemError::ErrorCode err;
        std::tie(server, err) = buildHttpServer(settings, requestHandler);
        if (!server)
            return std::make_tuple(nullptr, err);
    }

    if (!settings.ssl.endpoints.empty())
    {
        auto [secureServer, err] = buildHttpsServer(settings, requestHandler);
        if (!secureServer)
            return std::make_tuple(nullptr, err);

        SocketAddress httpsAddress = secureServer->endpoints()[0];
        if (!settings.serverName.empty())
            httpsAddress.address = SocketAddress(settings.serverName).address;
        else if (httpsAddress.address.toString() == "0.0.0.0")
            httpsAddress.address = HostAddress("127.0.0.1");

        if (server)
        {
            if (settings.redirectHttpToHttps)
            {
                server->forEachListener(
                    [httpsAddress = std::move(httpsAddress)](HttpStreamSocketServer* s)
                    {
                        s->redirectAllRequestsTo(std::move(httpsAddress));
                    });
            }

            server->append(std::move(secureServer));
        }
        else
        {
            server = std::move(secureServer);
        }
    }

    return {std::move(server), SystemError::noError};
}

std::unique_ptr<MultiEndpointServer> Builder::buildOrThrow(
    const Settings& settings,
    AbstractRequestHandler* requestHandler)
{
    std::unique_ptr<MultiEndpointServer> server;
    SystemError::ErrorCode err = SystemError::noError;
    std::tie(server, err) = build(settings, requestHandler);
    if (!server)
        throw std::system_error(err, std::system_category());

    return server;
}

std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> Builder::buildHttpServer(
    const Settings& settings,
    AbstractRequestHandler* requestHandler)
{
    auto server = std::make_unique<MultiEndpointServer>(requestHandler);

    if (!applySettings(settings, settings.endpoints, server.get()))
        return {nullptr, SystemError::getLastOSErrorCode()};
    configureServerUrls(settings, false, server.get());

    return {std::move(server), SystemError::noError};
}

std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> Builder::buildHttpsServer(
    const Settings& settings,
    AbstractRequestHandler* requestHandler)
{
    std::unique_ptr<HttpsServerContext> httpsContext;
    if (!settings.ssl.certificatePath.empty())
        httpsContext = std::make_unique<HttpsServerContext>(settings);

    auto server = std::make_unique<MultiEndpointSslServer>(
        requestHandler,
        std::move(httpsContext));

    if (!applySettings(settings, settings.ssl.endpoints, server.get()))
        return { nullptr, SystemError::getLastOSErrorCode() };
    configureServerUrls(settings, true, server.get());

    return {std::move(server), SystemError::noError};
}

bool Builder::applySettings(
    const Settings& settings,
    const std::vector<SocketAddress>& endpoints,
    MultiEndpointServer* httpServer)
{
    if (!httpServer->bind(endpoints))
        return false;

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

    return true;
}

void Builder::configureServerUrls(
    const Settings& settings,
    bool sslRequired,
    MultiEndpointServer* server)
{
    std::vector<nx::utils::Url> urls;
    server->forEachListener(
        [sslRequired, &urls](auto listener)
        {
            urls.push_back(url::Builder()
                .setScheme(sslRequired ? http::kSecureUrlSchemeName : http::kUrlSchemeName)
                .setEndpoint(listener->address()));
        });

    if (!settings.serverName.empty())
    {
        SocketAddress endpoint(settings.serverName);
        std::for_each(
            urls.begin(), urls.end(),
            [&endpoint](auto& url)
            {
                url.setHost(endpoint.address.toString());
                if (endpoint.port > 0)
                    url.setPort(endpoint.port);
            });
    }

    server->setUrls(std::move(urls));
}

} // namespace nx::network::http::server
