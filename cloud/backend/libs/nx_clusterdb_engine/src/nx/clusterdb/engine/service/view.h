#pragma once

#include <vector>

#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "controller.h"

namespace nx::clusterdb::engine {

class Settings;

class View
{
public:
    View(
        const Settings& settings,
        Controller* controller);

    std::vector<nx::network::SocketAddress> httpEndpoints() const;

    void listen();

private:
    nx::network::http::server::rest::MessageDispatcher m_httpMessageDispatcher;
    std::unique_ptr<nx::network::http::server::HttpServer> m_httpServer;
};

} // namespace nx::clusterdb::engine
