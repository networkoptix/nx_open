// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <map>
#include <optional>
#include <tuple>

#include <nx/network/socket_common.h>
#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/thread/mutex.h>

#include "http_types.h"
#include "server/abstract_authentication_manager.h"

namespace nx::network::http {

// TODO: #akolesnikov Move this class to http::GlobalContext in master branch.

/**
 * This cache is to help http clients to authenticate on server without receiving HTTP
 * Unauthorized error first.
 * Implemented on top of LRU cache.
 */
class NX_NETWORK_API AuthInfoCache
{
public:
    static constexpr auto kDefaultEntryLifeTime = std::chrono::minutes(5);
    static constexpr std::size_t kDefaultCacheSize = 100000;

    AuthInfoCache();

    /**
     * Complexity: ammortized O(log(N)).
     */
    std::optional<header::WWWAuthenticate> getServerResponse(
        const SocketAddress& serverEndpoint,
        server::Role serverRole,
        const std::string& userName);

    /**
     * Overrides existing entry, if any.
     */
    void cacheServerResponse(
        const SocketAddress& serverEndpoint,
        server::Role serverRole,
        const std::string& userName,
        header::WWWAuthenticate authenticateHeader);

    static AuthInfoCache& instance();

private:
    using Key = std::tuple<SocketAddress, server::Role, std::string>;

    nx::utils::TimeOutCache<Key, header::WWWAuthenticate, std::map> m_cache;
    nx::Mutex m_mutex;
};

} // namespace nx::network::http
