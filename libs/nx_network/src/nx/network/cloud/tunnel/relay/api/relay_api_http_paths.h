// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::cloud::relay::api {

static constexpr char kRelayProtocolName[] = "NXRELAY";
static constexpr char kRelayProtocolVersion[] = "0.1";
static constexpr char kRelayProtocol[] = "NXRELAY/0.1";

/**
 * Introducing this custom header since it is not correct to add "Upgrade" to any request
 * (e.g., when using GET / POST tunnel).
 */
static constexpr char kNxProtocolHeader[] = "Nx-Upgrade";

//-------------------------------------------------------------------------------------------------
// Server connection API.

static constexpr char kServerIncomingConnectionsPath[] =
    "/relay/server/{serverId}/incoming_connections/";

static constexpr char kServerTunnelBasePath[] = "/relay/server/{serverId}/tunnel";

static constexpr char kServerConnectionBasePath[] = "/relay/server/{serverId}/.*";

//-------------------------------------------------------------------------------------------------
// Client session API.

static constexpr char kServerClientSessionsPath[] = "/relay/server/{serverId}/client_sessions/";

static constexpr char kClientGetPostTunnelPath[] =
    "/relay/client_session/{sessionId}/tunnel/get_post/{sequence}";

static constexpr char kClientTunnelBasePath[] = "/relay/client_session/{sessionId}/tunnel";

static constexpr char kRelayClientSessionBasePath[] = "/relay/client_session/.*";

//-------------------------------------------------------------------------------------------------

static constexpr char kRelayClientPathPrefix[] = "/relay/client/";

static constexpr char kRelayStatisticsPath[] = "/relay/statistics";
static constexpr char kRelayStatisticsMetricsPath[] = "/relay/statistics/metrics/";
static constexpr char kRelayStatisticsSpecificMetricPath[] = "/relay/statistics/metrics/{metric}";

static constexpr char kDbRelaysPath[] = "/relay/maintenance/db/relays/";
static constexpr char kDbRelayPath[] = "/relay/maintenance/db/relays/{relayId}";

static constexpr char kServerAliasPath[] = "/relay/server/{serverId}/alias";

static constexpr char kApiPrefix[] = "/relay";

static constexpr char kApiDocPrefix[] = "/relay/docs/api";

static constexpr char kServerIdName[] = "serverId";
static constexpr char kSessionIdName[] = "sessionId";
static constexpr char kSequenceName[] = "sequence";
static constexpr char kMetricName[] = "metric";
static constexpr char kRelayIdName[] = "relayId";

} // namespace nx::cloud::relay::api
