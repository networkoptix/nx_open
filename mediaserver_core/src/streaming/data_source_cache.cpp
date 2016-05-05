////////////////////////////////////////////////////////////
// 13 may 2014    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "data_source_cache.h"

#include <nx/utils/thread/mutex.h>

#include "streaming_chunk_cache_key.h"


DataSourceCache::DataSourceCache()
:
    m_terminated( false )
{
}

const unsigned int DataSourceCache::DEFAULT_LIVE_TIME_MS;

DataSourceCache::~DataSourceCache()
{
    //removing all timers
        //only DataSourceCache::onTimer method can be called while we are here
    std::map<quint64, StreamingChunkCacheKey> timers;
    {
        QnMutexLocker lk( &m_mutex );
        m_terminated = true;
        std::swap( timers, m_timers );
    }

    for( auto val: timers )
        nx::utils::TimerManager::instance()->joinAndDeleteTimer( val.first );
}

DataSourceContextPtr DataSourceCache::take( const StreamingChunkCacheKey& key )
{
    nx::utils::TimerManager::TimerGuard timerGuard;
    QnMutexLocker lk( &m_mutex );

    //searching reader in cache
    for( auto it = m_cachedDataProviders.begin(); it != m_cachedDataProviders.end(); )
    {
        if( it->first.mediaStreamParamsEqualTo(key) &&
            it->first.live() == key.live() &&
            it->second.first->mediaDataProvider->currentPos() == key.startTimestamp() )
        {
            //taking existing reader which is already at required position (from previous chunk)
            DataSourceContextPtr item = it->second.first;
            timerGuard = nx::utils::TimerManager::TimerGuard( it->second.second );
            //timerGuard will remove timer after unlocking mutex
            m_timers.erase( timerGuard.get() );
            it = m_cachedDataProviders.erase( it );
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
    QnMutexLocker lk( &m_mutex );
    const quint64 timerID = nx::utils::TimerManager::instance()->addTimer(
        this,
        std::chrono::milliseconds(livetimeMs == 0 ? DEFAULT_LIVE_TIME_MS : livetimeMs));
    m_cachedDataProviders.emplace( key, std::make_pair( data, timerID ) );
    m_timers[timerID] = key;
}

void DataSourceCache::removeRange(
    const StreamingChunkCacheKey& beginKey,
    const StreamingChunkCacheKey& endKey )
{
    std::list<nx::utils::TimerManager::TimerGuard> timerGuards;
    QnMutexLocker lk( &m_mutex );
    for( auto
        it = m_cachedDataProviders.lower_bound( beginKey );
        it != m_cachedDataProviders.end() && (it->first < endKey);
         )
    {
        timerGuards.emplace_back( nx::utils::TimerManager::TimerGuard( it->second.second ) );
        it = m_cachedDataProviders.erase( it );
    }
}

void DataSourceCache::onTimer( const quint64& timerID )
{
    QnMutexLocker lk( &m_mutex );
    if( m_terminated )
        return;

    auto it = m_timers.find( timerID );
    if( it == m_timers.end() )
        return;
    m_cachedDataProviders.erase( it->second );
    m_timers.erase( it );
}
