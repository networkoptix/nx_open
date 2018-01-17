#include "relay_api_http_paths.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {

const char* const kRelayProtocolName = "NXRELAY/0.1";

const char* const kServerIncomingConnectionsPath = "/relay/server/{serverId}/incoming_connections/";
const char* const kServerClientSessionsPath = "/relay/server/{serverPeerName}/client_sessions/";
const char* const kClientSessionConnectionsPath = "/relay/client_session/{sessionId}/connections/";
const char* const kRelayClientPathPrefix = "/relay/client/";

const char* const kRelayStatisticsMetricsPath = "/relay/statistics/metrics/";
const char* const kRelayStatisticsSpecificMetricPath = "/relay/statistics/metrics/{metric}";

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
