#pragma once

#include <string>

#include <nx/utils/url.h>

#include "../socket_common.h"

namespace nx::network::url {

namespace detail {

NX_NETWORK_API std::string joinPath(
    const std::string& left,
    const std::string& right);

} // namespace detail

/**
 * @param scheme Comparison is done in low-case.
 * @return 0 for unknown scheme
 */
NX_NETWORK_API quint16 getDefaultPortForScheme(const QString& scheme);
NX_NETWORK_API SocketAddress getEndpoint(const nx::utils::Url&);

NX_NETWORK_API std::string normalizePath(const std::string&);
NX_NETWORK_API QString normalizePath(const QString&);
NX_NETWORK_API std::string normalizePath(const char*);

template<typename... Path>
std::string joinPath(
    const std::string& first,
    const std::string& second,
    const Path&... other)
{
    if constexpr (sizeof...(other) == 0)
        return detail::joinPath(first, second);
    else
        return joinPath(detail::joinPath(first, second), other...);
}

} // namespace nx::network::url
