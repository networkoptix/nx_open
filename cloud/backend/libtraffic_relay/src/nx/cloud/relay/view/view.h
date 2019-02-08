#pragma once

#include <memory>
#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/server/multi_endpoint_acceptor.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/http_server_base_authentication_manager.h>
#include <nx/network/http/server/http_server_htdigest_authentication_provider.h>
#include <nx/cloud/relaying/http_view/listening_peer_connection_tunneling.h>

#include "authentication_manager.h"
#include "client_connection_tunneling.h"

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
    View(
        const conf::Settings& settings,
        Model* model,
        Controller* controller);
    ~View();

    void registerStatisticsApiHandlers(const AbstractStatisticsProvider&);

    void start();

    std::vector<network::SocketAddress> httpEndpoints() const;
    std::vector<network::SocketAddress> httpsEndpoints() const;

    const nx::network::http::server::MultiEndpointAcceptor& httpServer() const;

private:
    /** Provides htdigest authentication for maintenance server*/
    struct MaintenanceAuthenticator
    {
        nx::network::http::server::HtdigestAuthenticationProvider provider;
        nx::network::http::server::BaseAuthenticationManager manager;

        MaintenanceAuthenticator(const std::string& maintenanceHtdigestPath):
            provider(maintenanceHtdigestPath),
            manager(&provider)
        {
        }
    };

    const conf::Settings& m_settings;
    Model* m_model;
    Controller* m_controller;
    relaying::ListeningPeerConnectionTunnelingServer m_listeningPeerConnectionTunnelingServer;
    view::ClientConnectionTunnelingServer m_clientConnectionTunnelingServer;
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx::network::http::AuthMethodRestrictionList m_authRestrictionList;
    view::AuthenticationManager m_authenticationManager;
    nx::network::http::server::AuthenticationDispatcher m_authenticationDispatcher;
    nx::network::http::server::MultiEndpointAcceptor m_multiAddressHttpServer;
    MaintenanceAuthenticator m_maintenanceAuthenticator;
    network::maintenance::Server m_maintenanceServer;

    void registerAuthenticators();

    void registerApiHandlers();

    template<typename Handler, typename ... Args>
    void registerApiHandler(
        const nx::network::http::StringType& method,
        Args... args);

    template<typename Handler, typename ... Args>
    void registerApiHandler(
        const char* path,
        const nx::network::http::StringType& method,
        Args... args);

    void initializeProxy();

    void loadSslCertificate();

    void startAcceptor();
};

} // namespace relay
} // namespace cloud
} // namespace nx
