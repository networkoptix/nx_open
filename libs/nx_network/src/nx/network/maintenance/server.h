#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

namespace nx::network::maintenance {

class NX_NETWORK_API Server
{
public:
    /* Registers all handlers under {basePath}/maintenance/… */
    void registerRequestHandlers(
        const std::string& basePath,
        http::server::rest::MessageDispatcher* messageDispatcher);
};

} // namespace nx::network::maintenance
