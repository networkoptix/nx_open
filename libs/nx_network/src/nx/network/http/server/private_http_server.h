// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nx/network/socket_common.h>

#include "multi_endpoint_server.h"
#include "rest/http_server_rest_message_dispatcher.h"

namespace nx::prometheus { class Registry; }

namespace nx::network::http::server {

/**
 * The HTTP server behind a service's private (internal-only) interface - the home for
 * operational endpoints (metics, health and etc.) that must not be exposed on the public API port
 * or subjected to its auth chain.
 */
class NX_NETWORK_API PrivateHttpServer
{
public:
    static inline const std::string kMetricsPath = "/metrics";

    PrivateHttpServer(nx::prometheus::Registry* registry, const SocketAddress& endpoint);

    bool listen();
    void pleaseStopSync();

    std::vector<SocketAddress> endpoints() const;

private:
    nx::prometheus::Registry* m_registry = nullptr;
    rest::MessageDispatcher m_dispatcher;
    std::unique_ptr<MultiEndpointServer> m_server;
};

} // namespace nx::network::http::server
