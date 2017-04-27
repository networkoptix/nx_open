#include "relay_api_http_paths.h"

namespace nx {
namespace cloud {
namespace relay {
namespace api {

/**
 * Parameter %1 is a server host name.
 */
const char* const kServerIncomingConnectionsPath = "/relay/server/{serverId}/incoming_connections/";
const char* const kServerClientSessionsPath = "/relay/server/{serverPeerName}/client_sessions/";
const char* const kClientSessionConnectionsPath = "/relay/client_session/{sessionId}/connections/";

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
