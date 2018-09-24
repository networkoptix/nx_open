#pragma once

namespace nx_http::tunneling::detail {

static constexpr char kGetPostTunnelPath[] = "/get_post/{sequence}";

static constexpr char kConnectionUpgradeTunnelPath[] = "/upgrade_connection";
static constexpr char kConnectionUpgradeTunnelProtocol[] = "NXTUNNEL";
static constexpr char kConnectionUpgradeTunnelMethod[] = "GET";

} // namespace nx_http::tunneling::detail
