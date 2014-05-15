////////////////////////////////////////////////////////////
// 13 may 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "data_source_cache.h"

#include <QtCore/QMutexLocker>

#include "streaming_chunk_cache_key.h"


DataSourceCache::DataSourceCache()
:
    m_terminated( false )
{
}

DataSourceCache::~DataSourceCache()
{
    //removing all timers
        //only DataSourceCache::onTimer method can be called while we are here
    {
        QMutexLocker lk( &m_mutex );
        m_terminated = true;
    }

    for( auto val: m_timers )
        TimerManager::instance()->joinAndDeleteTimer( val.first );
}

DataSourceContextPtr DataSourceCache::find( const StreamingChunkCacheKey& key )
{
    QMutexLocker lk( &m_mutex );

    //searching reader in cache
    for( auto it = m_cachedDataProviders.begin(); it != m_cachedDataProviders.end(); )
    {
        if( it->first.srcResourceUniqueID() == key.srcResourceUniqueID() &&
            it->first.channel() == key.channel() &&
            it->first.streamQuality() == key.streamQuality() &&
            it->first.pictureSizePixels() == key.pictureSizePixels() &&
            it->first.containerFormat() == key.containerFormat() &&
            it->first.videoCodec() == key.videoCodec() &&
            it->first.audioCodec() == key.audioCodec() &&
            it->first.live() == key.live() &&
            it->second->mediaDataProvider->currentPos() == key.startTimestamp() )
        {
            //taking existing reader which is already at required position (from previous chunk)
            DataSourceContextPtr item = it->second;
            m_cachedDataProviders.erase( it++ );
            return item;
        }
        else
        {
            ++it;
        }
    }

    return DataSourceContextPtr();
}

void DataSourceCache::put(
    const StreamingChunkCacheKey& key,
    DataSourceContextPtr data,
    const unsigned int livetimeMs )
{
    QMutexLocker lk( &m_mutex );
    m_cachedDataProviders.insert( std::make_pair( key, data ) );
    const quint64 timerID = TimerManager::instance()->addTimer( this, livetimeMs == 0 ? DEFAULT_LIVE_TIME_MS : livetimeMs );
    m_timers[timerID] = key;
}

void DataSourceCache::onTimer( const quint64& timerID )
{
    QMutexLocker lk( &m_mutex );
    if( m_terminated )
        return;

    auto it = m_timers.find( timerID );
    if( it == m_timers.end() )
        return;
    m_cachedDataProviders.erase( it->second );
    m_timers.erase( it );
}
