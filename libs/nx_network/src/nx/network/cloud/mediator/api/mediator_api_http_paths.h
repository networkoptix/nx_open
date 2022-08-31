// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::hpm::api {

static constexpr char kMediatorApiPrefix[] = "/mediator";

static constexpr char kStatisticsApiPrefix[] = "/statistics";

static constexpr char kSystemId[] = "systemId";
static constexpr char kServerId[] = "serverId";

static constexpr char kStatisticsMetricsPath[] = "/statistics/metrics/";
static constexpr char kStatisticsListeningPeersPath[] = "/statistics/listening-peers/";
static constexpr char kStatisticsSystemPeersPath[] = "/statistics/system/{systemId}/servers/";

static constexpr char kStunOverHttpTunnelPath[] = "/stun_tunnel_auth";

static constexpr char kServerSessionsPath[] = "/server/{hostname}/sessions/";

static constexpr char kConnectionSpeedUplinkPath[] = "/connection_speed/uplink";
static constexpr char kConnectionSpeedUplinkPathV2[] =
    "/system/{systemId}/server/{serverId}/connection-speed/uplink";

static constexpr char kApiDocPrefix[] = "/mediator/docs/api";

} // namespace nx::hpm::api
