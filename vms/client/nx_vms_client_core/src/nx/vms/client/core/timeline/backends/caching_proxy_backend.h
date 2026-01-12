// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>
#include <map>
#include <optional>

#include <QtCore/QDeadlineTimer>
#include <QtCore/QPointer>
#include <QtCore/QPromise>

#include <nx/utils/log/assert.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/mutex.h>
#include <utils/common/synctime.h>

#include "abstract_proxy_backend.h"
#include "data_lists_cache.h"

namespace nx::vms::client::core {
namespace timeline {

/**
 * A proxy backend that caches time period blocks.
 *
 * When a load is requested, it first attempts to *fully* load the requested period from the cache.
 * If the requested period is present in the cache, it's returned synchronously, otherwise a load
 * of the whole period is requested from the source backend, and the results are added to the cache
 * (potentially overwriting previously cached intersecting periods) and returned asyncronously.
 *
 * The cache maintains a linked list of time period blocks. Accessed blocks are moved to the top.
 * If the maximum number of blocks (`maximumBlockCount`) or the maximum total number of data items
 * (`maximumCacheSize`) is exceeded, the least recently accessed bottom of the list is discarded.
 *
 * The cache supports expiration by time, controlled by `expirationTime` and `ageThreshold`.
 * If a time period block is expired by time, it is eventually discarded.
 *
 * The following expiration policies can be configured:
 *  1) `expirationTime` is not set: all blocks are not expirable, `ageThreshold` is ignored.
 *  2) `expirationTime` is set, `ageThreshold` is not set: all blocks are expirable.
 *  3) `expirationTime` is set, `ageThreshold` is set: blocks younger than `ageThreshold` are
 *     expirable, blocks older than `ageThreshold` are not expirable. See `ageThreshold` notes.
 *
 * See `DataListsCache<DataList>` notes for more details of the current cache implementation.
 */
template<typename DataList>
class CachingProxyBackend: public AbstractProxyBackend<DataList>
{
public:
    explicit CachingProxyBackend(
        const BackendPtr<DataList>& source, QnSyncTime* timeSource = QnSyncTime::instance());

    /**
     * Request asynchronous data load.
     */
    virtual QFuture<DataList> load(const QnTimePeriod& period, int limit) override;

    /**
     * A maximum total number of data items the cache is allowed to hold.
     * Default 5000.
     */
    int maximumCacheSize() const;
    void setMaximumCacheSize(int value);

    /**
     * A maximum number of data blocks the cache is allowed to hold.
     * Default 1000.
     */
    int maximumBlockCount() const;
    void setMaximumBlockCount(int value);

    /**
     * A duration after which the expirable cached blocks expire.
     * Default 30s.
     */
    std::optional<std::chrono::seconds> expirationTime() const;
    void setExpirationTime(std::optional<std::chrono::seconds> value);

    /**
     * An age threshold after which cached blocks are considered not expirable.
     * Default: 1h.
     *
     * An age of a block is a difference between `m_timeSource->value()` at the moment of the block
     * addition to the cache and the block's end timestamp.
     */
    std::optional<std::chrono::seconds> ageThreshold() const;
    void setAgeThreshold(std::optional<std::chrono::seconds> value);

private:
    std::optional<std::chrono::seconds> effectiveExpirationTime(
        std::chrono::milliseconds blockEndTime) const;

private:
    QPointer<QnSyncTime> m_timeSource;
    std::optional<std::chrono::seconds> m_expirationTime{std::chrono::seconds(30)};
    std::optional<std::chrono::seconds> m_ageThreshold{std::chrono::hours(1)};
    DataListsCache<DataList> m_cache;
    mutable nx::Mutex m_mutex;
};

// ------------------------------------------------------------------------------------------------
// CachingProxyBackend<DataList> implementation.

template<typename DataList>
CachingProxyBackend<DataList>::CachingProxyBackend(
    const BackendPtr<DataList>& source,
    QnSyncTime* timeSource)
    :
    AbstractProxyBackend<DataList>(source),
    m_timeSource(timeSource)
{
}

template<typename DataList>
QFuture<DataList> CachingProxyBackend<DataList>::load(const QnTimePeriod& period, int limit)
{
    if (!NX_ASSERT(!period.isEmpty() && limit > 0)
        || !this->source()
        || !NX_ASSERT(this->shared_from_this()))
    {
        return {};
    }

    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        if (auto fromCache = m_cache.get(period, limit))
        {
            NX_VERBOSE(this, "Fetched data from the cache for [%1, %2), limit=%3",
                period.startTimeMs, period.endTimeMs(), limit);

            QPromise<DataList> promise;
            promise.start();
            promise.emplaceResult(std::move(*fromCache));
            promise.finish();
            return promise.future();
        }
    }

    NX_VERBOSE(this, "No complete data in the cache for [%1, %2), limit=%3; requesting.",
        period.startTimeMs, period.endTimeMs(), limit);

    return this->source()->load(period, limit).then(
        [this, period, limit, weakGuard = this->weak_from_this()](DataList result) -> DataList
        {
            const auto guard = weakGuard.lock();
            if (!guard)
                return {};

            const auto expirationTime = effectiveExpirationTime(period.endTime());

            NX_VERBOSE(this, "Received response for [%1, %2), limit=%3, received=%4; "
                "adding to the cache (expirationTime=%5)",
                period.startTime(), period.endTime(), limit, result.size(), expirationTime);

            NX_MUTEX_LOCKER lk(&m_mutex);
            m_cache.add(period, limit, result, expirationTime);
            return result;
        });
}

template<typename DataList>
int CachingProxyBackend<DataList>::maximumCacheSize() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_cache.maximumCacheSize();
}

template<typename DataList>
int CachingProxyBackend<DataList>::maximumBlockCount() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_cache.maximumBlockCount();
}

template<typename DataList>
void CachingProxyBackend<DataList>::setMaximumCacheSize(int value)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_cache.setMaximumCacheSize(value);
}

template<typename DataList>
void CachingProxyBackend<DataList>::setMaximumBlockCount(int value)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_cache.setMaximumBlockCount(value);
}

template<typename DataList>
std::optional<std::chrono::seconds> CachingProxyBackend<DataList>::expirationTime() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_expirationTime;
}

template<typename DataList>
void CachingProxyBackend<DataList>::setExpirationTime(std::optional<std::chrono::seconds> value)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_expirationTime = value;
}

template<typename DataList>
std::optional<std::chrono::seconds> CachingProxyBackend<DataList>::ageThreshold() const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    return m_ageThreshold;
}

template<typename DataList>
void CachingProxyBackend<DataList>::setAgeThreshold(std::optional<std::chrono::seconds> value)
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    m_ageThreshold = value;
}

template<typename DataList>
std::optional<std::chrono::seconds> CachingProxyBackend<DataList>::effectiveExpirationTime(
    std::chrono::milliseconds blockEndTime) const
{
    NX_MUTEX_LOCKER lk(&m_mutex);
    if (!m_expirationTime)
        return {};

    if (!m_ageThreshold || !NX_ASSERT(m_timeSource))
        return m_expirationTime;

    const auto age = m_timeSource->value() - blockEndTime;
    return (age < *m_ageThreshold) ? m_expirationTime : std::optional<std::chrono::seconds>{};
}

} // namespace timeline
} // namespace nx::vms::client::core
