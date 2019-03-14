#pragma once

namespace nx {
namespace network {
namespace cloud {

enum class ConnectType
{
    unknown = 0,
    forwardedTcpPort = 1 << 0,    /**< E.g., Upnp. */
    udpHp = 1 << 1,               /**< UDP hole punching. */
    tcpHp = 1 << 2,               /**< TCP hole punching. */
    proxy = 1 << 3,               /**< Proxy server address. */
    all = forwardedTcpPort | udpHp | tcpHp | proxy,
};

} // namespace cloud
} // namespace network
} // namespace nx
