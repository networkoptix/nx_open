// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_host_priority.h"

#include <nx/network/address_resolver.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>

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

} // namespace nx::vms::common
