// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

#include <nx/utils/url.h>

#include "../socket_common.h"

namespace nx::network::url {

namespace detail {

NX_NETWORK_API std::string joinPath(
    const std::string_view& left,
    const std::string_view& right);

} // namespace detail

/**
 * @param scheme Comparison is done in low-case.
 * @return 0 for unknown scheme
 */
NX_NETWORK_API std::uint16_t getDefaultPortForScheme(const std::string_view& scheme);
NX_NETWORK_API SocketAddress getEndpoint(const nx::utils::Url& url, int defaultPort);
NX_NETWORK_API SocketAddress getEndpoint(const nx::utils::Url& url);

NX_NETWORK_API std::string normalizePath(const std::string_view&);

// TODO: #akolesnikov Merge this function with the previous one.
NX_NETWORK_API QString normalizedPath(
    const QString& path, const QString& pathIgnorePrefix = QString());

template<typename... Path>
std::string joinPath(
    const std::string_view& first,
    const std::string_view& second,
    const Path&... other)
{
    if constexpr (sizeof...(other) == 0)
        return detail::joinPath(first, second);
    else
        return joinPath(detail::joinPath(first, second), other...);
}

} // namespace nx::network::url
