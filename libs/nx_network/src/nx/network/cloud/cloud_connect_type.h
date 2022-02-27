// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/reflect/enum_instrument.h>

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

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(ConnectType*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<ConnectType>;
    return visitor(
        Item{ConnectType::unknown, "unknown"},
        Item{ConnectType::forwardedTcpPort, "tcp"},
        Item{ConnectType::udpHp, "udpHp"},
        Item{ConnectType::tcpHp, "tcpHp"},
        Item{ConnectType::proxy, "proxy"},
        Item{ConnectType::all, "all"}
    );
}

} // namespace nx::network::cloud
