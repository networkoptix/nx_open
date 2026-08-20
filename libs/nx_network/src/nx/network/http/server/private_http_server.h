// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nx/network/socket_common.h>
#include <nx/reflect/instrument.h>

#include "health_monitor.h"
#include "multi_endpoint_server.h"
#include "rest/http_server_rest_message_dispatcher.h"

namespace nx::prometheus { class Registry; }

namespace nx::network::http::server {

/** What the health endpoints answer with. */
struct HealthReply
{
    NX_REFLECTION_ENUM_CLASS_IN_CLASS(Status, ok, unavailable)

    Status status = Status::ok;

    /** Check name -> failure reason. */
    std::optional<std::map<std::string, std::string>> failed;
};

NX_REFLECTION_INSTRUMENT(HealthReply, (status)(failed))

/**
 * The HTTP server behind a service's private (internal-only) interface - the home for
 * operational endpoints (metics, health and etc.) that must not be exposed on the public API port
 * or subjected to its auth chain.
 *
 * - /metrics: the Prometheus service registry metrics.
 * - /livez: 200 while the main listener answers, 503 once it does not.
 * - /readyz: 200 once every registered readiness probe passes, 503 naming those that do not.
 * - /malloc_info and /debug/counters: the in-process diagnostics (/debug/pprof analogue).
 *
 * The health endpoints are always served, whether or not the service registered any readiness
 * probe. They answer {"status": "ok"} while healthy and
 * {"status": "unavailable", "failed": {check: reason}} otherwise.
 */
class NX_NETWORK_API PrivateHttpServer
{
public:
    static inline const std::string kMetricsPath = "/metrics";
    static inline const std::string kLivenessPath = "/livez";
    static inline const std::string kReadinessPath = "/readyz";

    PrivateHttpServer(nx::prometheus::Registry* registry, const SocketAddress& endpoint);

    /** NOTE: listen() starts the monitor, so its setup has to be done before that call. */
    HealthMonitor& healthMonitor();

    bool listen();
    void pleaseStopSync();

    std::vector<SocketAddress> endpoints() const;

private:
    nx::prometheus::Registry* m_registry = nullptr;
    HealthMonitor m_healthMonitor;
    rest::MessageDispatcher m_dispatcher;
    std::unique_ptr<MultiEndpointServer> m_server;
};

} // namespace nx::network::http::server
