#pragma once

namespace nx::network::http::tunneling::detail {

static constexpr char kGetPostTunnelPath[] = "/get_post/{sequence}";

static constexpr char kConnectionUpgradeTunnelPath[] = "/upgrade_connection";
static constexpr char kConnectionUpgradeTunnelProtocol[] = "NXTUNNEL";
static constexpr char kConnectionUpgradeTunnelMethod[] = "GET";

} // namespace nx::network::http::tunneling::detail
