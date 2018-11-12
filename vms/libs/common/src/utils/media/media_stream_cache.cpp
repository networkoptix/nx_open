////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "media_stream_cache.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <cstdlib>
#include <algorithm>

//#define DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
#include <malloc.h>
#endif

#include "detail/media_stream_cache_detail.h"


using namespace std;

MediaStreamCache::SequentialReadContext::SequentialReadContext(
    MediaStreamCache* cache,
    quint64 startTimestamp )
:
    m_sharedCache( cache->m_sharedImpl ),
    m_startTimestamp( startTimestamp ),
    m_firstFrame( true ),
    m_currentTimestamp( startTimestamp ),
    m_blockingID( -1 )
{
    m_blockingID = cache->m_sharedImpl->blockData( startTimestamp );
}

MediaStreamCache::SequentialReadContext::~SequentialReadContext()
{
    auto strongCacheRef = m_sharedCache.lock();
    if( !strongCacheRef )
        return;

    strongCacheRef->unblockData( m_blockingID );
    m_blockingID = -1;
}

//!If no next frame returns NULL
QnAbstractDataPacketPtr MediaStreamCache::SequentialReadContext::getNextFrame()
{
    auto strongCacheRef = m_sharedCache.lock();
    if( !strongCacheRef )
        return QnAbstractDataPacketPtr();

    if( m_firstFrame )
    {
        QnAbstractDataPacketPtr packet = strongCacheRef->findByTimestamp(
            m_startTimestamp,
            true,
            &m_currentTimestamp );
        if( !packet )
            return QnAbstractDataPacketPtr();
        m_firstFrame = false;
        return packet;
    }

    QnAbstractDataPacketPtr packet = strongCacheRef->getNextPacket( m_currentTimestamp, &m_currentTimestamp );
    if( packet )
        strongCacheRef->moveBlocking( m_blockingID, m_currentTimestamp );
    return packet;
}

//!Returns timestamp of previous packet
quint64 MediaStreamCache::SequentialReadContext::currentPos() const
{
    return m_currentTimestamp;
}


MediaStreamCache::MediaStreamCache(
    unsigned int cacheSizeMillis,
    unsigned int maxCacheSizeMillis)
:
    m_sharedImpl(
        std::make_shared<detail::MediaStreamCache>(
            cacheSizeMillis,
            maxCacheSizeMillis))
{
}

MediaStreamCache::~MediaStreamCache()
{
}

//!Implementation of QnAbstractMediaDataReceptor::canAcceptData
bool MediaStreamCache::canAcceptData() const
{
    return m_sharedImpl->canAcceptData();
}

//!Implementation of QnAbstractMediaDataReceptor::putData
void MediaStreamCache::putData( const QnAbstractDataPacketPtr& data )
{
    m_sharedImpl->putData( data );
}

void MediaStreamCache::clear()
{
    m_sharedImpl->clear();
}

quint64 MediaStreamCache::startTimestamp() const
{
    return m_sharedImpl->startTimestamp();
}

quint64 MediaStreamCache::currentTimestamp() const
{
    return m_sharedImpl->currentTimestamp();
}

qint64 MediaStreamCache::duration() const
{
    return m_sharedImpl->duration();
}

size_t MediaStreamCache::sizeInBytes() const
{
    return m_sharedImpl->sizeInBytes();
}

int MediaStreamCache::getMaxBitrate() const
{
    return m_sharedImpl->getMaxBitrate();
}

//!Returns packet with timestamp == \a timestamp or packet with closest (from the left) timestamp
/*!
    In other words, this methods performs lower_bound in timestamp-sorted (using 'greater' comparision) array of data packets
*/
QnAbstractDataPacketPtr MediaStreamCache::findByTimestamp(
    quint64 desiredTimestamp,
    bool findKeyFrameOnly,
    quint64* const foundTimestamp ) const
{
    return m_sharedImpl->findByTimestamp(
        desiredTimestamp,
        findKeyFrameOnly,
        foundTimestamp );
}

QnAbstractDataPacketPtr MediaStreamCache::getNextPacket( quint64 timestamp, quint64* const foundTimestamp ) const
{
    return m_sharedImpl->getNextPacket( timestamp, foundTimestamp );
}

nx::utils::Subscription<quint64 /*frameTimestampUsec*/>& MediaStreamCache::keyFrameFoundSubscription()
{
    return m_sharedImpl->keyFrameFoundSubscription();
}

nx::utils::Subscription<>& MediaStreamCache::streamTimeDiscontinuityFoundSubscription()
{
    return m_sharedImpl->streamTimeDiscontinuityFoundSubscription();
}

int MediaStreamCache::blockData( quint64 timestamp )
{
    return m_sharedImpl->blockData( timestamp );
}

void MediaStreamCache::moveBlocking( int blockingID, quint64 timestampToMoveTo )
{
    return m_sharedImpl->moveBlocking( blockingID, timestampToMoveTo );
}

void MediaStreamCache::unblockData( int blockingID )
{
    m_sharedImpl->unblockData( blockingID );
}

qint64 MediaStreamCache::inactivityPeriod() const
{
    return m_sharedImpl->inactivityPeriod();
}

#endif // ENABLE_DATA_PROVIDERS
