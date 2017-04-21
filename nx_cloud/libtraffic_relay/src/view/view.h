#pragma once

#include <memory>
#include <vector>

#include <nx/network/connection_server/multi_address_server.h>
#include <nx/network/auth_restriction_list.h>
#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/server/http_stream_socket_server.h>

#include "authentication_manager.h"

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }
class Controller;
class Model;

class View
{
public:
    View(
        const conf::Settings& settings,
        const Model& model,
        Controller* controller);
    ~View();

    void start();

    std::vector<SocketAddress> httpEndpoints() const;

private:
    const conf::Settings& m_settings;
    nx_http::MessageDispatcher m_httpMessageDispatcher;
    QnAuthMethodRestrictionList m_authRestrictionList;
    view::AuthenticationManager m_authenticationManager;
    std::unique_ptr<MultiAddressServer<nx_http::HttpStreamSocketServer>> m_multiAddressHttpServer;
};

} // namespace relay
} // namespace cloud
} // namespace nx
