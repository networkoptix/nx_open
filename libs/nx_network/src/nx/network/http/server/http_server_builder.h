// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "multi_endpoint_server.h"
#include "settings.h"

namespace nx::prometheus { class Registry; }

namespace nx::network::http::server {

class AbstractAuthenticationManager;

/**
 * Builds HTTP server.
 */
class NX_NETWORK_API Builder
{
public:
    /**
     * Builds HTTP server based on settings.
     * @return Server that is bound to specified endpoints, but does not listen yet.
     *
     * Notes on SSL: if settings specify certificatePath then an SSL context is created
     * and is used with the SSL listener(s). If no certificatePath was specified, then
     * the default SSL context (ssl::Context::instance()) is used.
     * SSL context (if initialized) is encapsulated in the returned object.
     *
     * If metricsRegistry is provided, the server records golden-signal Prometheus metrics
     * for each request (see MetricsRequestHandler). The serverName is used for metric labeling;
     * an empty value omits the server-name label.
     */
    static std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> build(
        const Settings& settings,
        AbstractRequestHandler* requestHandler,
        nx::prometheus::Registry* metricsRegistry = nullptr,
        std::string serverName = "");

    /**
     * Same as Builder::build, but throws on error.
     * NOTE: Throws on error.
     */
    static std::unique_ptr<MultiEndpointServer> buildOrThrow(
        const Settings& settings,
        AbstractRequestHandler* requestHandler,
        nx::prometheus::Registry* metricsRegistry = nullptr,
        std::string serverName = "");

private:
    struct Context;

    static std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> buildHttpServer(
        const Settings& settings,
        AbstractRequestHandler* requestHandler);

    static std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> buildHttpsServer(
        const Settings& settings,
        AbstractRequestHandler* requestHandler);

    static bool applySettings(Context* ctx, const std::vector<SocketAddress>& endpoints);
    static bool bindServer(Context* ctx, const std::vector<SocketAddress>& endpoints);
    static bool configureListener(Context* ctx, HttpStreamSocketServer* listener);
    static bool reuseEndpointsForConcurrency(Context* ctx);
    static void configureServerUrls(Context* ctx, bool sslRequired);
};

} // namespace nx::network::http::server
