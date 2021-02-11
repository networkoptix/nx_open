#include "auth_cache.h"

#include <cinttypes>

#include <nx/utils/std/algorithm.h>

#include "auth_tools.h"

namespace nx::network::http {

AuthInfoCache::AuthInfoCache():
    m_cache(kDefaultEntryLifeTime, kDefaultCacheSize)
{
}

std::optional<header::WWWAuthenticate> AuthInfoCache::getServerResponse(
    const SocketAddress& serverEndpoint,
    server::Role serverRole,
    const StringType& userName)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_cache.getValue(std::tie(serverEndpoint, serverRole, userName));
}

/**
 * Overrides existing entry, if any.
 */
void AuthInfoCache::cacheServerResponse(
    const SocketAddress& serverEndpoint,
    server::Role serverRole,
    const StringType& userName,
    header::WWWAuthenticate authenticateHeader)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    return m_cache.put(
        std::tie(serverEndpoint, serverRole, userName),
        std::move(authenticateHeader));
}

AuthInfoCache& AuthInfoCache::instance()
{
    static AuthInfoCache authInfoCacheInstance;
    return authInfoCacheInstance;
}

} // namespace nx::network::http
