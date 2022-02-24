// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/connection_server/multi_address_server.h>

#include "multi_endpoint_server.h"
#include "rest/http_server_rest_message_dispatcher.h"
#include "settings.h"

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
     */
    static std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> build(
        const Settings& settings,
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* messageDispatcher);

    /**
     * Same as Builder::build, but throws on error.
     * NOTE: Throws on error.
     */
    static std::unique_ptr<MultiEndpointServer> buildOrThrow(
        const Settings& settings,
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* messageDispatcher);

private:
    static std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> buildHttpServer(
        const Settings& settings,
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* httpMessageDispatcher);

    static std::tuple<std::unique_ptr<MultiEndpointServer>, SystemError::ErrorCode> buildHttpsServer(
        const Settings& settings,
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* httpMessageDispatcher);

    static bool applySettings(
        const Settings& settings,
        const std::vector<SocketAddress>& endpoints,
        MultiEndpointServer* httpServer);

    static void configureServerUrls(
        const Settings& settings,
        bool sslRequired,
        MultiEndpointServer* httpServer);
};

} // namespace nx::network::http::server
