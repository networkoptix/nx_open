#pragma once

#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/connection_server/multi_address_server.h>

#include "settings.h"

namespace nx::network::http::server {

namespace rest { class MessageDispatcher; }

class AbstractAuthenticationManager;

class HttpServer:
    public nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>
{
    using base_type =
        nx::network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

public:
    template<typename... Others>
    HttpServer(
        HttpServer&& one,
        Others&& ... others)
        :
        base_type(std::move(one), std::move(others)...)
    {
    }

    HttpServer(
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* messageDispatcher,
        bool sslEnabled,
        NatTraversalSupport natTraversalSupport)
        :
        base_type(
            authenticator,
            messageDispatcher,
            sslEnabled,
            natTraversalSupport)
    {
    }

    void setTcpBackLogSize(int tcpBackLogSize)
    {
        m_tcpBackLogSize = tcpBackLogSize;
    }

    void listen()
    {
        base_type::listen(m_tcpBackLogSize);
    }

private:
    int m_tcpBackLogSize = Settings::kDefaultTcpBacklogSize;
};

//-------------------------------------------------------------------------------------------------

class NX_NETWORK_API Builder
{
public:
    /**
     * Builds HTTP server based on settings.
     * @return Server that is bound to specified endpoints, but does not listen yet.
     * NOTE: Throws on error.
     */
    static std::unique_ptr<HttpServer> build(
        const Settings& settings,
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* messageDispatcher);

private:
    static std::unique_ptr<HttpServer> buildServer(
        const Settings& settings,
        bool sslRequired,
        const std::vector<SocketAddress>& endpoints,
        AbstractAuthenticationManager* authenticator,
        rest::MessageDispatcher* httpMessageDispatcher);

    static void applySettings(
        const Settings& settings,
        const std::vector<SocketAddress>& endpoints,
        HttpServer* httpServer);
};

} // namespace nx::network::http::server
