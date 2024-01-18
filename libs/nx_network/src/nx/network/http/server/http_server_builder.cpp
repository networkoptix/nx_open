// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_server_builder.h"

#include <nx/network/url/url_builder.h>

#include "https_server_context.h"

namespace nx::network::http::server {

struct Builder::Context
{
    const Settings& settings;
    MultiEndpointServer* server = nullptr;
    /** Effective concurrency. */
    std::size_t listeningConcurrency = 0;
    bool reusePort = false;

    std::vector<aio::AbstractAioThread*> aioThreads;
    std::vector<aio::AbstractAioThread*>::iterator nextAioThreadIt;

    Context(const Settings& settings, MultiEndpointServer* server):
        settings(settings), server(server) {}
};

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

    Context context(settings, server.get());
    if (!applySettings(&context, settings.endpoints))
        return {nullptr, SystemError::getLastOSErrorCode()};
    configureServerUrls(&context, false);

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

    Context context(settings, server.get());
    if (!applySettings(&context, settings.ssl.endpoints))
        return {nullptr, SystemError::getLastOSErrorCode()};
    configureServerUrls(&context, true);

    return {std::move(server), SystemError::noError};
}

bool Builder::applySettings(
    Context* ctx,
    const std::vector<SocketAddress>& endpoints)
{
    ctx->listeningConcurrency = ctx->settings.listeningConcurrency == 0
        ? std::thread::hardware_concurrency() : ctx->settings.listeningConcurrency;
    ctx->reusePort = ctx->settings.reusePort || ctx->listeningConcurrency > endpoints.size();
    ctx->aioThreads = SocketGlobals::instance().aioService().getAllAioThreads();
    ctx->nextAioThreadIt = ctx->aioThreads.begin();

    if (!bindServer(ctx, endpoints))
    {
        NX_WARNING(typeid(Builder), "error binding HTTP server to %1", endpoints);
        return false;
    }

    if (ctx->listeningConcurrency > endpoints.size())
    {
        if (!reuseEndpointsForConcurrency(ctx))
        {
            NX_WARNING(typeid(Builder), "error reusing endpoints %1 for concurrency", endpoints);
            return false;
        }
    }

    if (ctx->settings.connectionInactivityPeriod > std::chrono::milliseconds::zero())
    {
        ctx->server->forEachListener(
            [ctx](nx::network::http::HttpStreamSocketServer* server)
            {
                server->setConnectionInactivityTimeout(
                    ctx->settings.connectionInactivityPeriod);
            });
    }

    ctx->server->setTcpBackLogSize(ctx->settings.tcpBacklogSize);
    ctx->server->setExtraResponseHeaders(ctx->settings.extraResponseHeaders);

    return true;
}

bool Builder::bindServer(Context* ctx, const std::vector<SocketAddress>& endpoints)
{
    if (!ctx->server->bind(
            endpoints,
            [ctx](auto* listener) { return configureListener(ctx, listener); }))
    {
        NX_WARNING(typeid(Builder), "error binding HTTP server to %1", endpoints);
        return false;
    }

    return true;
}

bool Builder::configureListener(Context* ctx, HttpStreamSocketServer* listener)
{
    if (!listener->setReusePort(ctx->reusePort))
    {
        NX_WARNING(typeid(Builder), "could not set reuse port flag: %1. Proceeding further anyway...",
            SystemError::getLastOSErrorText());
    }

    listener->bindToAioThread(*ctx->nextAioThreadIt);
    if ((++ctx->nextAioThreadIt) == ctx->aioThreads.end())
        ctx->nextAioThreadIt = ctx->aioThreads.begin();

    return true;
}

bool Builder::reuseEndpointsForConcurrency(Context* ctx)
{
    std::vector<SocketAddress> endpoints;
    ctx->server->forEachListener([&endpoints](HttpStreamSocketServer* listener) {
        endpoints.push_back(listener->address());
    });

    auto endpointIt = endpoints.begin();
    for (std::size_t i = 0; i + endpoints.size() < ctx->listeningConcurrency; ++i)
    {
        if (!bindServer(ctx, {*endpointIt}))
        {
            NX_WARNING(typeid(Builder), "error reusing endpoint %1 for concurrency", *endpointIt);
            return false;
        }

        if ((++endpointIt) == endpoints.end())
            endpointIt = endpoints.begin();
    }

    return true;
}

void Builder::configureServerUrls(Context* ctx, bool sslRequired)
{
    std::vector<nx::utils::Url> urls;
    ctx->server->forEachListener(
        [sslRequired, &urls](auto listener)
        {
            urls.push_back(url::Builder()
                .setScheme(sslRequired ? http::kSecureUrlSchemeName : http::kUrlSchemeName)
                .setEndpoint(listener->address()));
        });

    if (!ctx->settings.serverName.empty())
    {
        SocketAddress endpoint(ctx->settings.serverName);
        std::for_each(
            urls.begin(), urls.end(),
            [&endpoint](auto& url)
            {
                url.setHost(endpoint.address.toString());
                if (endpoint.port > 0)
                    url.setPort(endpoint.port);
                if (endpoint.port == 443)
                    url.setScheme("https");
            });
    }

    ctx->server->setUrls(std::move(urls));
}

} // namespace nx::network::http::server
