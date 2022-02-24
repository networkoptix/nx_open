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
    Server(const std::string& basePath);

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
    log::Server m_logServer;
    statistics::Server m_statisticsServer;
    std::string m_maintenancePath;
};

} // namespace nx::network::maintenance
