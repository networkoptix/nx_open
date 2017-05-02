#include "http_handlers.h"

#include <nx/network/cloud/tunnel/relay/api/relay_api_http_paths.h>

namespace nx {
namespace cloud {
namespace relay {
namespace view {

const char* BeginListeningHandler::kPath = api::kServerIncomingConnectionsPath;

const char* CreateClientSessionHandler::kPath = api::kServerClientSessionsPath;

const char* ConnectToPeerHandler::kPath = api::kClientSessionConnectionsPath;

} // namespace view
} // namespace relay
} // namespace cloud
} // namespace nx
