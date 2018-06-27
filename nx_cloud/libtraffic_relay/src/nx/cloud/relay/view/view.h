#pragma once

#include <memory>
#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "authentication_manager.h"
#include "get_post_tunnel_processor.h"

namespace nx {
namespace cloud {
namespace relay {

class AbstractStatisticsProvider;

namespace conf { class Settings; }
class Controller;
class Model;

class View
{
public:
    using MultiHttpServer =
        network::server::MultiAddressServer<nx::network::http::HttpStreamSocketServer>;

    View(
        const conf::Settings& settings,
        Model* model,
        Controller* controller);
    ~View();

    void registerStatisticsApiHandlers(const AbstractStatisticsProvider&);

    void start();

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

    const MultiHttpServer& httpServer() const;

private:
    const conf::Settings& m_settings;
    Model* m_model;
    Controller* m_controller;
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx::network::http::AuthMethodRestrictionList m_authRestrictionList;
    view::AuthenticationManager m_authenticationManager;
    std::unique_ptr<MultiHttpServer> m_multiAddressHttpServer;
    std::vector<network::SocketAddress> m_httpEndpoint;
    std::vector<network::SocketAddress> m_httpsEndpoint;
    view::GetPostServerTunnelProcessor m_getPostServerTunnelProcessor;
    view::GetPostClientTunnelProcessor m_getPostClientTunnelProcessor;

    void registerApiHandlers();
    void registerCompatibilityHandlers();

    template<typename Handler, typename ... Arg>
    void registerApiHandler(
        const nx::network::http::StringType& method,
        Arg... arg);

    template<typename Handler, typename ... Arg>
    void registerApiHandler(
        const char* path,
        const nx::network::http::StringType& method,
        Arg... arg);

    void loadSslCertificate();

    void startAcceptor();

    std::unique_ptr<MultiHttpServer> startHttpServer(
        const std::list<network::SocketAddress>& endpoints);

    std::unique_ptr<MultiHttpServer> startHttpsServer(
        const std::list<network::SocketAddress>& endpoints);

    std::unique_ptr<MultiHttpServer> startServer(
        const std::list<network::SocketAddress>& endpoints,
        bool sslMode);
};

} // namespace relay
} // namespace cloud
} // namespace nx
