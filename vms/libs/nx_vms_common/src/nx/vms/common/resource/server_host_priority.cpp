// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_host_priority.h"

#include <nx/network/address_resolver.h>
#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_builder.h>

namespace nx::vms::common {

ServerHostPriority serverHostPriority(const nx::network::HostAddress& host)
{
    if (host.isLoopback())
        return ServerHostPriority::localHost;

    if (host.isLocalNetwork())
        return host.ipV4() ? ServerHostPriority::localIpV4 : ServerHostPriority::localIpV6;

    if (host.ipV4() || (bool) host.ipV6().first)
        return ServerHostPriority::remoteIp;

    if (nx::network::SocketGlobals::addressResolver().isCloudHostname(host.toString()))
        return ServerHostPriority::cloud;

    return ServerHostPriority::dns;
}

nx::utils::Url mainServerUrl(const QSet<QString>& remoteAddresses,
    std::function<int(const nx::utils::Url&)> priority)
{
    std::map<nx::utils::Url, int> addresses;
    for (const auto& addressString: remoteAddresses)
    {
        auto str = addressString.toStdString();
        nx::network::SocketAddress sockAddr(addressString.toStdString());

        nx::utils::Url url = nx::network::url::Builder()
            .setScheme(nx::network::http::kSecureUrlSchemeName)
            .setEndpoint(sockAddr)
            .toUrl();

        addresses[url] =
            priority ? priority(url) : static_cast<int>(serverHostPriority(url.host()));
    }

    if (addresses.empty())
        return {};

    auto itr = std::min_element(addresses.cbegin(), addresses.cend(),
        [](const auto& l, const auto& r) { return l.second < r.second; });

    return itr->first;
}

} // namespace nx::vms::common
