// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "address_resolver.h"
#include "socket_global.h"

namespace nx::network {

struct DnsAlias
{
    const nx::network::SocketAddress original;
    const nx::network::SocketAddress alias;

    DnsAlias(nx::network::SocketAddress original_, nx::network::HostAddress alias_):
        original(original_), alias(alias_, original_.port)
    {
        nx::network::SocketGlobals::addressResolver().addFixedAddress(alias.address, original);
    }

    ~DnsAlias()
    {
        nx::network::SocketGlobals::addressResolver().removeFixedAddress(alias.address, original);
    }

    DnsAlias(const DnsAlias&) = delete;
    DnsAlias& operator=(const DnsAlias&) = delete;
};

} // namespace nx::network
