#pragma once

#include <map>
#include <memory>

#include <QtCore/QSharedPointer>

#include <nx/utils/timer_manager.h>
#include <nx/utils/thread/mutex.h>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <transcoding/transcoder.h>

#include "streaming_chunk_cache_key.h"

class DataSourceContext
{
public:
    AbstractOnDemandDataProviderPtr mediaDataProvider;
    std::unique_ptr<QnTranscoder> transcoder;

    DataSourceContext(
        AbstractOnDemandDataProviderPtr _mediaDataProvider,
        std::unique_ptr<QnTranscoder> _transcoder)
        :
        mediaDataProvider(_mediaDataProvider),
        transcoder(std::move(_transcoder))
    {
    }
};

using DataSourceContextPtr = std::shared_ptr<DataSourceContext>;

class DataSourceCache:
    public nx::utils::TimerEventHandler
{
public:
    static const unsigned int DEFAULT_LIVE_TIME_MS = 3 * 1000;

    DataSourceCache(nx::utils::StandaloneTimerManager* timeManager);
    virtual ~DataSourceCache();

    DataSourceContextPtr take(const StreamingChunkCacheKey& key);
    void put(
        const StreamingChunkCacheKey& key,
        DataSourceContextPtr data,
        const int unsigned livetimeMs = DEFAULT_LIVE_TIME_MS);
    /**
     * Removes keys within range [beginKey, endKey).
     */
    void removeRange(
        const StreamingChunkCacheKey& beginKey,
        const StreamingChunkCacheKey& endKey);

    virtual void onTimer(const quint64& timerID) override;

private:
    nx::utils::StandaloneTimerManager* m_timeManager;
    mutable QnMutex m_mutex;
    std::map<StreamingChunkCacheKey, std::pair<DataSourceContextPtr, quint64>> m_cachedDataProviders;
    std::map<quint64, StreamingChunkCacheKey> m_timers;
    bool m_terminated;
};
