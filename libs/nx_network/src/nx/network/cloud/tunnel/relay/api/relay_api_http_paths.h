#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace api {

static constexpr char kRelayProtocolName[] = "NXRELAY";
static constexpr char kRelayProtocolVersion[] = "0.1";
static constexpr char kRelayProtocol[] = "NXRELAY/0.1";

/**
 * Introducing this custom header since it is not correct to add "Upgrade" to any request
 * (e.g., when using GET / POST tunnel).
 */
static constexpr char kNxProtocolHeader[] = "Nx-Upgrade";

static constexpr char kServerIncomingConnectionsPath[] =
    "/relay/server/{serverId}/incoming_connections/";

static constexpr char kServerTunnelPath[] = "/relay/server/{serverId}/tunnel/get_post/{sequence}";
static constexpr char kServerTunnelBasePath[] = "/relay/server/{serverId}/tunnel";

static constexpr char kServerClientSessionsPath[] = "/relay/server/{serverId}/client_sessions/";
static constexpr char kClientSessionConnectionsPath[] =
    "/relay/client_session/{sessionId}/connections/";

static constexpr char kClientGetPostTunnelPath[] =
    "/relay/client_session/{sessionId}/tunnel/get_post/{sequence}";

static constexpr char kClientTunnelBasePath[] = "/relay/client_session/{sessionId}/tunnel";
static constexpr char kRelayClientPathPrefix[] = "/relay/client/";

static constexpr char kRelayStatisticsMetricsPath[] = "/relay/statistics/metrics/";
static constexpr char kRelayStatisticsSpecificMetricPath[] = "/relay/statistics/metrics/{metric}";

static constexpr char kApiPrefix[] = "/relay";

static constexpr char kServerIdName[] = "serverId";
static constexpr char kSessionIdName[] = "sessionId";
static constexpr char kSequenceName[] = "sequence";
static constexpr char kMetricName[] = "metric";

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
