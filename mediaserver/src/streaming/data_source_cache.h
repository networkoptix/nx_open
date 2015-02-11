////////////////////////////////////////////////////////////
// 13 may 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef DATA_SOURCE_CACHE_H
#define DATA_SOURCE_CACHE_H

#include <map>

#include <utils/common/mutex.h>
#include <QtCore/QSharedPointer>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <transcoding/transcoder.h>
#include <utils/common/timermanager.h>

#include "streaming_chunk_cache_key.h"


class DataSourceContext
{
public:
    AbstractOnDemandDataProviderPtr mediaDataProvider;
    QnTranscoderPtr transcoder;

    DataSourceContext(
        AbstractOnDemandDataProviderPtr _mediaDataProvider,
        QnTranscoderPtr _transcoder )
    :
        mediaDataProvider( _mediaDataProvider ),
        transcoder( _transcoder )
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

    DataSourceContextPtr find( const StreamingChunkCacheKey& key );
    void put(
        const StreamingChunkCacheKey& key,
        DataSourceContextPtr data,
        const int unsigned livetimeMs = DEFAULT_LIVE_TIME_MS );

    //!Implementation of TimerEventHandler::onTimer
    virtual void onTimer( const quint64& timerID ) override;

private:
    mutable QnMutex m_mutex;
    std::map<StreamingChunkCacheKey, DataSourceContextPtr> m_cachedDataProviders;
    std::map<quint64, StreamingChunkCacheKey> m_timers;
    bool m_terminated;
};

#endif  //DATA_SOURCE_CACHE_H
