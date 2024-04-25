// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::hpm::api {

static constexpr char kMediatorApiPrefix[] = "/mediator";

static constexpr char kStatisticsApiPrefix[] = "/statistics";

static constexpr char kSystemId[] = "systemId";
static constexpr char kServerId[] = "serverId";

static constexpr char kStatisticsMetricsPath[] = "/statistics/metrics/";
static constexpr char kStatisticsListeningPeersPath[] = "/statistics/listening-peers";
static constexpr char kStatisticsSystemPeersPath[] = "/statistics/system/{systemId}/servers/";

/**
 * This path requires HTTP authentication. So, each STUN request does not have to provide
 * any signature.
 */
static constexpr char kStunOverHttpTunnelPath[] = "/stun_tunnel_auth";

static constexpr char kServerSessionsPath[] = "/server/{hostname}/sessions/";

static constexpr char kConnectionSpeedUplinkPath[] = "/connection_speed/uplink";
static constexpr char kConnectionSpeedUplinkPathV2[] =
    "/system/{systemId}/server/{serverId}/connection-speed/uplink";

static constexpr char kSystemResetPath[] = "maintenance/system/{systemId}/reset";

static constexpr char kApiDocPrefix[] = "/mediator/docs/api";

static constexpr char kRelaysPath[] = "/maintenance/relays";
static constexpr char kRelayPath[] = "/maintenance/relays/{id}";
static constexpr char kRelayAttrsPath[] = "/maintenance/relays/{id}/attrs";

static constexpr char kIdParam[] = "id";

static constexpr char kRuleSetsPath[] = "/rule-sets";
static constexpr char kRuleSetPath[] =  "/rule-sets/{ruleSetId}";
static constexpr char kRulesPath[] =    "/rule-sets/{ruleSetId}/rules";
static constexpr char kRulePath[] =     "/rule-sets/{ruleSetId}/rules/{ruleId}";

static constexpr char kRuleSetId[] = "ruleSetId";
static constexpr char kRuleId[] = "ruleId";

namespace deprecated {

/**
 * This path does not require HTTP authentication. But, each STUN request has to provide
 * valid signature.
 */
static constexpr char kStunOverHttpTunnelPath[] = "/stun_tunnel";

} // namespace deprecated

} // namespace nx::hpm::api
