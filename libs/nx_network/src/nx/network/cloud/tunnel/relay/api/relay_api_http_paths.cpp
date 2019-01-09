#include "relay_api_http_paths.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {

const char* const kRelayProtocolName = "NXRELAY/0.1";

const char* const kServerIncomingConnectionsPath = "/relay/server/{serverId}/incoming_connections/";
const char* const kServerTunnelPath = "/relay/server/{serverId}/tunnel/get_post/{sequence}";
const char* const kServerTunnelBasePath = "/relay/server/{serverId}/tunnel";

const char* const kServerClientSessionsPath = "/relay/server/{serverId}/client_sessions/";
const char* const kClientSessionConnectionsPath = "/relay/client_session/{sessionId}/connections/";
const char* const kClientGetPostTunnelPath =
    "/relay/client_session/{sessionId}/tunnel/get_post/{sequence}";
const char* const kClientTunnelBasePath = "/relay/client_session/{sessionId}/tunnel";
const char* const kRelayClientPathPrefix = "/relay/client/";

const char* const kRelayStatisticsMetricsPath = "/relay/statistics/metrics/";
const char* const kRelayStatisticsSpecificMetricPath = "/relay/statistics/metrics/{metric}";

const char* const kApiPrefix = "/relay";

const char* const kServerIdName = "serverId";
const char* const kSessionIdName = "sessionId";
const char* const kSequenceName = "sequence";
const char* const kMetricName = "metric";

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
