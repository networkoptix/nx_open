#pragma once

namespace nx {
namespace network {
namespace cloud {

enum class ConnectType
{
    unknown = 0,
    forwardedTcpPort = 0x01,    /**< E.g., Upnp. */
    udpHp = 0x02,               /**< UDP hole punching. */
    tcpHp = 0x04,               /**< TCP hole punching. */
    proxy = 0x08,               /**< Proxy server address. */
    all = forwardedTcpPort | udpHp | tcpHp | proxy,
};

} // namespace cloud
} // namespace network
} // namespace nx
