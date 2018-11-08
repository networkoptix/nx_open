#include "data_source_cache.h"

#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>

#include "streaming_chunk_cache_key.h"

const unsigned int DataSourceCache::DEFAULT_LIVE_TIME_MS;

DataSourceCache::DataSourceCache(nx::utils::StandaloneTimerManager* timerManager):
    m_timerManager(timerManager),
    m_terminated(false)
{
}

DataSourceCache::~DataSourceCache()
{
    // Removing all timers.
    // Only DataSourceCache::onTimer method can be called while we are here.
    std::map<quint64, StreamingChunkCacheKey> timers;
    {
        QnMutexLocker lock(&m_mutex);
        m_terminated = true;
        std::swap(timers, m_timers);
    }

    for (auto val: timers)
        m_timerManager->joinAndDeleteTimer(val.first);
}

DataSourceContextPtr DataSourceCache::take(const StreamingChunkCacheKey& key)
{
    nx::utils::TimerManager::TimerGuard timerGuard;
    QnMutexLocker lock(&m_mutex);

    // Searching reader in cache.
    for (auto it = m_cachedDataProviders.begin(); it != m_cachedDataProviders.end(); )
    {
        const bool isTranscoderSuitable =
            it->first.live() == key.live() &&
            it->first.mediaStreamParamsEqualTo(key) &&
            it->first.streamingSessionId() == key.streamingSessionId() &&
            !key.streamingSessionId().isEmpty();
        const bool mediaDataProviderSuitable =
            it->first.live() == key.live() &&
            it->second.first->mediaDataProvider->currentPos() == key.startTimestamp();

        if (!isTranscoderSuitable)
        {
            NX_VERBOSE(this,
                lm("Existing data provider (live %1, pos %2) does not fit (live %3, pos %4)")
                .arg(it->first.live()).arg(it->second.first->mediaDataProvider->currentPos())
                .arg(key.live()).arg(key.startTimestamp()));
            ++it;
            continue;
        }

        // Taking existing reader which is already at required position (from previous chunk).
        DataSourceContextPtr item = it->second.first;
        timerGuard = nx::utils::TimerManager::TimerGuard(
            m_timerManager,
            it->second.second);
        // timerGuard will remove timer after unlocking mutex.
        m_timers.erase(timerGuard.get());
        m_cachedDataProviders.erase(it);

        if (!mediaDataProviderSuitable)
        {
            NX_VERBOSE(this, lm("Returning chunk transcoder only. resourceId %1, requested pos %2")
                .arg(key.srcResourceUniqueID()).arg(key.startTimestamp()));
            item->mediaDataProvider.reset();
        }

        return item;
    }

    return nullptr;
}

void DataSourceCache::put(
    const StreamingChunkCacheKey& key,
    DataSourceContextPtr data,
    const unsigned int livetimeMs)
{
    QnMutexLocker lock(&m_mutex);
    const quint64 timerId = m_timerManager->addTimer(
        this,
        std::chrono::milliseconds(livetimeMs == 0 ? DEFAULT_LIVE_TIME_MS : livetimeMs));
    m_cachedDataProviders.emplace(key, std::make_pair(data, timerId));
    m_timers[timerId] = key;
}

void DataSourceCache::removeRange(
    const StreamingChunkCacheKey& beginKey,
    const StreamingChunkCacheKey& endKey)
{
    std::list<nx::utils::TimerManager::TimerGuard> timerGuards;
    QnMutexLocker lock(&m_mutex);
    for (auto
        it = m_cachedDataProviders.lower_bound(beginKey);
        it != m_cachedDataProviders.end() && (it->first < endKey);
        )
    {
        timerGuards.emplace_back(
            nx::utils::TimerManager::TimerGuard(
                m_timerManager,
                it->second.second));
        it = m_cachedDataProviders.erase(it);
    }
}

void DataSourceCache::onTimer(const quint64& timerId)
{
    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;

    auto it = m_timers.find(timerId);
    if (it == m_timers.end())
        return;
    m_cachedDataProviders.erase(it->second);
    m_timers.erase(it);
}
