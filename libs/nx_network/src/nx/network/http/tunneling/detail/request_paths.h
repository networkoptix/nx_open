#pragma once

namespace nx::network::http::tunneling::detail {

static constexpr char kGetPostTunnelPath[] = "/get_post/{sequence}";

static constexpr char kConnectionUpgradeTunnelPath[] = "/upgrade_connection";
static constexpr char kConnectionUpgradeTunnelProtocol[] = "NXTUNNEL";
static constexpr char kConnectionUpgradeTunnelMethod[] = "GET";

static constexpr char kExperimentalTunnelUpPath[] = "/experimental/{tunnelId}/up";
static constexpr char kExperimentalTunnelDownPath[] = "/experimental/{tunnelId}/down";

static constexpr char kMultiMessageTunnelDownPath[] = "/multi_message/{tunnelId}/down";
static constexpr char kMultiMessageTunnelUpPath[] = "/multi_message/{tunnelId}/up/{sequence}";

static constexpr char kTunnelIdName[] = "tunnelId";
static constexpr char kTunnelMessageSequenceName[] = "sequence";

} // namespace nx::network::http::tunneling::detail
