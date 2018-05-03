#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace api {

NX_NETWORK_API extern const char* const kRelayProtocolName;

NX_NETWORK_API extern const char* const kServerIncomingConnectionsPath;
NX_NETWORK_API extern const char* const kServerClientSessionsPath;
NX_NETWORK_API extern const char* const kClientSessionConnectionsPath;
NX_NETWORK_API extern const char* const kRelayClientPathPrefix;

NX_NETWORK_API extern const char* const kRelayStatisticsMetricsPath;
NX_NETWORK_API extern const char* const kRelayStatisticsSpecificMetricPath;

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
