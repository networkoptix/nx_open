#include "nonce_cache.h"

namespace nx::network::http {

NonceCache::NonceCache():
    m_cache(kDefaultEntryLifetime, kDefaultCacheSize)
{
}

int NonceCache::getNonceCount(
    const SocketAddress& serverEndpoint,
    server::Role serverRole,
    const StringType& nonce)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto key = std::tie(serverEndpoint, serverRole, nonce);

    auto val = m_cache.getValue(key);
    if (val)
    {
        ++(*val);
        return *val;
    }

    m_cache.put(std::move(key), 1);
    return 1;
}

NonceCache& NonceCache::instance()
{
    static NonceCache nonceCacheInstance;
    return nonceCacheInstance;
}

} // namespace nx::network::http
