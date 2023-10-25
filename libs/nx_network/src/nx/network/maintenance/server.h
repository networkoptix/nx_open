// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>

#include "log/server.h"
#include "statistics/server.h"

namespace nx::network::maintenance {

class NX_NETWORK_API Server
{
public:
    /**
     * @param basePath is a request path prefix used to form the server api endpoints
     * {basePath}/maintenance/...
     * @param version optionally overrides the VMS version that is returned by the
     * .../maintenance/version endpoint.
     */
    Server(const std::string& basePath, std::optional<std::string> version = std::nullopt);

    /** Registers all handlers under {basePath}/maintenance/ given in constructor*/
    void registerRequestHandlers(
        http::server::rest::MessageDispatcher* messageDispatcher);

    /**
     * Get the path that this server would register its request handlers under.
     *
     * @return a string containing {basePath}/maintenance
     */
    std::string maintenancePath() const;

private:
    std::string m_maintenancePath;
    std::optional<std::string> m_version;
    log::Server m_logServer;
    statistics::Server m_statisticsServer;
};

} // namespace nx::network::maintenance
