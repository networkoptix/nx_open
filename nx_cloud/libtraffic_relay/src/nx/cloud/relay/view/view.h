#pragma once

#include <memory>
#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/http/auth_restriction_list.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "authentication_manager.h"

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
        network::server::MultiAddressServer<nx_http::HttpStreamSocketServer>;

    View(
        const conf::Settings& settings,
        const Model& model,
        Controller* controller);
    ~View();

    void registerStatisticsApiHandlers(const AbstractStatisticsProvider&);

    void start();

    std::vector<SocketAddress> httpEndpoints() const;

    const MultiHttpServer& httpServer() const;

private:
    using MultiHttpServerPtr = std::unique_ptr<MultiHttpServer>;

    const conf::Settings& m_settings;
    Controller* m_controller;
    nx_http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    nx_http::AuthMethodRestrictionList m_authRestrictionList;
    view::AuthenticationManager m_authenticationManager;
    MultiHttpServerPtr m_multiAddressHttpServer;

    void registerApiHandlers();
    void registerCompatibilityHandlers();

    template<typename Handler, typename Arg>
    void registerApiHandler(
        const nx_http::StringType& method,
        Arg arg);

    template<typename Handler, typename Arg>
    void registerApiHandler(
        const char* path,
        const nx_http::StringType& method,
        Arg arg);

    void startAcceptor();
};

} // namespace relay
} // namespace cloud
} // namespace nx
