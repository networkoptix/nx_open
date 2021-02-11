#pragma once

#include <chrono>
#include <map>
#include <tuple>

#include <nx/network/socket_common.h>
#include <nx/utils/data_structures/time_out_cache.h>
#include <nx/utils/thread/mutex.h>

#include "http_types.h"
#include "server/abstract_authentication_manager.h"

namespace nx::network::http {

// TODO: #ak Move this class to http::GlobalContext in master branch.

/**
 * Maintains a nonce usage count.
 * Uses LRU cache internally.
 */
class NonceCache
{
public:
    static constexpr auto kDefaultEntryLifetime = std::chrono::minutes(5);
    static constexpr int kDefaultCacheSize = 100000;

    NonceCache();

    /**
     * Each call increases sequence number associated with
     * (serverEndpoint; serverRole; nonce) tuple.
     * If no sequence associated with the specified key found, it is inserted with a value 1
     * which is returned.
     * Complexity: O(log(N)).
     */
    int getNonceCount(
        const SocketAddress& serverEndpoint,
        server::Role serverRole,
        const StringType& nonce);

    static NonceCache& instance();

private:
    using Key = std::tuple<SocketAddress, server::Role, StringType>;

    nx::utils::TimeOutCache<Key, int, std::map> m_cache;
    nx::Mutex m_mutex;
};

} // namespace nx::network::http
