////////////////////////////////////////////////////////////
// 13 may 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef DATA_SOURCE_CACHE_H
#define DATA_SOURCE_CACHE_H

#include <map>
#include <memory>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QSharedPointer>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <transcoding/transcoder.h>
#include <nx/utils/timermanager.h>

#include "streaming_chunk_cache_key.h"


class DataSourceContext
{
public:
    AbstractOnDemandDataProviderPtr mediaDataProvider;
    std::unique_ptr<QnTranscoder> transcoder;

    DataSourceContext(
        AbstractOnDemandDataProviderPtr _mediaDataProvider,
        std::unique_ptr<QnTranscoder> _transcoder )
    :
        mediaDataProvider( _mediaDataProvider ),
        transcoder( std::move(_transcoder) )
    {
    }
};
typedef QSharedPointer<DataSourceContext> DataSourceContextPtr;

class DataSourceCache
:
    public TimerEventHandler
{
public:
    static const unsigned int DEFAULT_LIVE_TIME_MS = 3*1000;

    DataSourceCache();
    virtual ~DataSourceCache();

    DataSourceContextPtr take( const StreamingChunkCacheKey& key );
    void put(
        const StreamingChunkCacheKey& key,
        DataSourceContextPtr data,
        const int unsigned livetimeMs = DEFAULT_LIVE_TIME_MS );
    //!Removes keys within range [beginKey, endKey)
    void removeRange(
        const StreamingChunkCacheKey& beginKey,
        const StreamingChunkCacheKey& endKey );

    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID ) override;

private:
    mutable QnMutex m_mutex;
    std::map<StreamingChunkCacheKey, std::pair<DataSourceContextPtr, quint64>> m_cachedDataProviders;
    std::map<quint64, StreamingChunkCacheKey> m_timers;
    bool m_terminated;
};

#endif  //DATA_SOURCE_CACHE_H
