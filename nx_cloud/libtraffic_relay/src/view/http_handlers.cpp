#include "http_handlers.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

const char* BeginListeningHandler::kPath = api::path::kServerIncomingConnections;

const char* CreateClientSessionHandler::kPath = api::path::kServerClientSessions;

const char* ConnectToPeerHandler::kPath = api::path::kClientSessionConnections;

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
