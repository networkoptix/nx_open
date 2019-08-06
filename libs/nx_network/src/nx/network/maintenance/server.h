#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "log/server.h"

namespace nx::network::maintenance {

class NX_NETWORK_API Server
{
public:
    /* Registers all handlers under {basePath}/maintenance/… */
    void registerRequestHandlers(
        const std::string& basePath,
        http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * Get the path that this server registered its request handlers under.
     * NOTE: not available until after registerRequestHandlers is called.
     *
     * @return a string containing {basePath}/maintenance
     */
    std::string maintenancePath() const;

private:
    log::Server m_logServer;
    std::string m_maintenancePath;
};

} // namespace nx::network::maintenance
