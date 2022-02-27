// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>

#include <nx/network/http/http_types.h>
#include <nx/network/socket_common.h>
#include <nx/utils/thread/mutex.h>

namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

class RunTimeOptions
{
public:
    void enforceSsl(const network::SocketAddress& address, bool enabled = true);
    bool isSslEnforced(const network::SocketAddress& address) const;

    void enforceHeaders(const network::SocketAddress& address, nx::network::http::HttpHeaders headers);
    nx::network::http::HttpHeaders enforcedHeaders(const network::SocketAddress& address) const;

private:
    mutable nx::Mutex m_mutex;
    std::set<network::SocketAddress> m_enforcedSslAddresses;
    std::map<network::SocketAddress, nx::network::http::HttpHeaders> m_enforcedHeadersForAddress;
};

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx
