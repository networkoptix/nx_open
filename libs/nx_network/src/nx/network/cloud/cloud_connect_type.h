#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::network::cloud {

enum class ConnectType
{
    unknown = 0,
    forwardedTcpPort = 1 << 0,    /**< E.g., Upnp. */
    udpHp = 1 << 1,               /**< UDP hole punching. */
    tcpHp = 1 << 2,               /**< TCP hole punching. */
    proxy = 1 << 3,               /**< Proxy server address. */
    all = forwardedTcpPort | udpHp | tcpHp | proxy,
};

} // namespace nx::network::cloud

QN_FUSION_DECLARE_FUNCTIONS(nx::network::cloud::ConnectType, (lexical), NX_NETWORK_API)
