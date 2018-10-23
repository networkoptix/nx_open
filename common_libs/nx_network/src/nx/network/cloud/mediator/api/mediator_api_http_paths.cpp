#include "mediator_api_http_paths.h"

namespace nx {
namespace hpm {
namespace api {

const char* const kMediatorApiPrefix = "/mediator";

const char* const kStatisticsListeningPeersPath = "/statistics/listening_peers";
const char* const kStunOverHttpTunnelPath = "/stun_tunnel";

const char* const kServerSessionsPath = "/server/{hostname}/sessions/";

const char* const kStatisticsMetricsPath = "/statistics/metrics/";

} // namespace api
} // namespace hpm
} // namespace nx
