// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "run_time_options.h"

namespace nx {
namespace cloud {
namespace gateway {
namespace conf {

void RunTimeOptions::enforceSsl(const network::SocketAddress& address, bool enabled)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (enabled)
        m_enforcedSslAddresses.insert(address);
    else
        m_enforcedSslAddresses.erase(address);
}

bool RunTimeOptions::isSslEnforced(const network::SocketAddress& address) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_enforcedSslAddresses.find(address) != m_enforcedSslAddresses.end();
}

void RunTimeOptions::enforceHeaders(
    const network::SocketAddress& address, nx::network::http::HttpHeaders headers)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (headers.empty())
        m_enforcedHeadersForAddress.erase(address);
    else
        m_enforcedHeadersForAddress[address] = std::move(headers);
}

nx::network::http::HttpHeaders RunTimeOptions::enforcedHeaders(
    const network::SocketAddress& address) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    const auto it = m_enforcedHeadersForAddress.find(address);
    return it == m_enforcedHeadersForAddress.end() ? nx::network::http::HttpHeaders{} : it->second;
}

} // namespace conf
} // namespace cloud
} // namespace gateway
} // namespace nx
