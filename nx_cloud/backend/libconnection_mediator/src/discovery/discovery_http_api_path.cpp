#include "discovery_http_api_path.h"

namespace nx {
namespace cloud {
namespace discovery {
namespace http {

const char* const kDiscoveredModulesPath = "/mediator/discovery/modules/";
const char* const kDiscoveredModulePath = "/mediator/discovery/module/{moduleId}";
const char* const kModuleKeepAliveConnectionPath = "/mediator/discovery/module/{moduleId}/keepalive";

} // namespace http
} // namespace discovery
} // namespace cloud
} // namespace nx
