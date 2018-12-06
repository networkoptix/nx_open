#pragma once

namespace nx {
namespace cloud {
namespace relay {
namespace api {

NX_NETWORK_API extern const char* const kRelayProtocolName;

NX_NETWORK_API extern const char* const kServerIncomingConnectionsPath;
NX_NETWORK_API extern const char* const kServerTunnelPath;
NX_NETWORK_API extern const char* const kServerTunnelBasePath;

NX_NETWORK_API extern const char* const kServerClientSessionsPath;
NX_NETWORK_API extern const char* const kClientSessionConnectionsPath;
NX_NETWORK_API extern const char* const kClientGetPostTunnelPath;
NX_NETWORK_API extern const char* const kClientTunnelBasePath;
NX_NETWORK_API extern const char* const kRelayClientPathPrefix;

NX_NETWORK_API extern const char* const kRelayStatisticsMetricsPath;
NX_NETWORK_API extern const char* const kRelayStatisticsSpecificMetricPath;

NX_NETWORK_API extern const char* const kServerIdName;
NX_NETWORK_API extern const char* const kSessionIdName;
NX_NETWORK_API extern const char* const kSequenceName;
NX_NETWORK_API extern const char* const kMetricName;

} // namespace api
} // namespace relay
} // namespace cloud
} // namespace nx
