#include "relay_api_http_paths.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {
namespace path {

/**
 * Parameter %1 is a server host name.
 */
const char* const kServerIncomingConnections = "/relay/server/{serverId}/incoming_connections/";
const char* const kServerClientSessions = "/relay/server/{serverPeerName}/client_sessions/";
const char* const kClientSessionConnections = "/relay/client_session/{sessionId}/connections/";

} // namespace path
} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
