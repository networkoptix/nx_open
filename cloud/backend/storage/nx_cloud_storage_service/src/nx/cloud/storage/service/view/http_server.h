#pragma once

#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/multi_endpoint_acceptor.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::cloud::storage::service {

class Settings;

class HttpServer
{
public:
    HttpServer(const Settings& settings);

    network::http::server::MultiEndpointAcceptor& server();
    network::http::server::rest::MessageDispatcher& messageDispatcher();

    bool bind();
    bool listen();
    void stop();

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

private:
    void registerApiHandlers();

private:
    const Settings& m_settings;
    network::http::server::rest::MessageDispatcher m_messageDispatcher;
    network::http::server::AuthenticationDispatcher m_authenticationDispatcher;
    network::http::server::MultiEndpointAcceptor m_multiAddressServer;
};

} // namespace nx::cloud::storage::service