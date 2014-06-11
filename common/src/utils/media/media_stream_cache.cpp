////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "media_stream_cache.h"

#include <algorithm>

#include <QMutexLocker>

#include "mediaindex.h"


using namespace std;

static const int USEC_PER_SEC = 1000*1000;

MediaStreamCache::SequentialReadContext::SequentialReadContext(
    MediaStreamCache* cache,
    quint64 startTimestamp )
:
    m_cache( cache ),
    m_startTimestamp( startTimestamp ),
    m_firstFrame( true ),
    m_currentTimestamp( startTimestamp ),
    m_blockingID( -1 )
{
    m_blockingID = m_cache->blockData( startTimestamp );
}

MediaStreamCache::SequentialReadContext::~SequentialReadContext()
{
    m_cache->unblockData( m_blockingID );
    m_blockingID = -1;
}

//!If no next frame returns NULL
QnAbstractDataPacketPtr MediaStreamCache::SequentialReadContext::getNextFrame()
{
    if( m_firstFrame )
    {
        QnAbstractDataPacketPtr packet = m_cache->findByTimestamp(
            m_startTimestamp,
            true,
            &m_currentTimestamp );
        if( !packet )
            return QnAbstractDataPacketPtr();
        m_firstFrame = false;
        return packet;
    }

    QnAbstractDataPacketPtr packet = m_cache->getNextPacket( m_currentTimestamp, &m_currentTimestamp );
    if( packet )
        m_cache->moveBlocking( m_blockingID, m_currentTimestamp );
    return packet;
}

//!Returns timestamp of previous packet
quint64 MediaStreamCache::SequentialReadContext::currentPos() const
{
    return m_currentTimestamp;
}


MediaStreamCache::MediaStreamCache(
    unsigned int cacheSizeMillis,
    MediaIndex* const mediaIndex )
:
    m_cacheSizeMillis( cacheSizeMillis ),
    m_mediaIndex( mediaIndex ),
    m_mutex( QMutex::Recursive ),
    m_prevPacketSrcTimestamp( -1 ),
    m_currentPacketTimestamp( 0 ),
    m_cacheSizeInBytes( 0 )
{
}

//!Implementation of QnAbstractDataReceptor::canAcceptData
bool MediaStreamCache::canAcceptData() const
{
    return true;
}

static qint64 MAX_ALLOWED_TIMESTAMP_DIFF = 5*1000*1000UL;  //5 seconds
static const int MICROS_PER_MS = 1000;

//!Implementation of QnAbstractDataReceptor::putData
void MediaStreamCache::putData( QnAbstractDataPacketPtr data )
{
    QMutexLocker lk( &m_mutex );

    QnAbstractMediaDataPtr mediaPacket = data.dynamicCast<QnAbstractMediaData>();
    const bool isKeyFrame = mediaPacket && (mediaPacket->flags & QnAbstractMediaData::MediaFlags_AVKey);
    if( m_packetsByTimestamp.empty() && !isKeyFrame )
        return; //cache data MUST start with key frame

    //calculating timestamp, because we cannot rely on data->timestamp, since it may contain discontinuity in case of reconnect to camera
    if( m_prevPacketSrcTimestamp == -1 )
    {
        m_prevPacketSrcTimestamp = data->timestamp;
        m_currentPacketTimestamp = data->timestamp;
    }

    if( qAbs(data->timestamp - m_prevPacketSrcTimestamp) > MAX_ALLOWED_TIMESTAMP_DIFF )
    {
        //timestamp discontinuity
        m_prevPacketSrcTimestamp = data->timestamp;
    }

    if( data->timestamp > m_prevPacketSrcTimestamp )
    {
        m_currentPacketTimestamp += data->timestamp - m_prevPacketSrcTimestamp;
        m_prevPacketSrcTimestamp = data->timestamp;
    }

    if( isKeyFrame )
        m_mediaIndex->addPoint( m_currentPacketTimestamp, QDateTime::currentDateTimeUtc(), isKeyFrame );

    if( m_packetsByTimestamp.insert( make_pair( m_currentPacketTimestamp, make_pair( data, isKeyFrame ) ) ).second )
    {
        QnAbstractMediaDataPtr mediaPacket = data.dynamicCast<QnAbstractMediaData>();
        if( mediaPacket )
            m_cacheSizeInBytes += mediaPacket->data.size();
    }

    if( !isKeyFrame )
        return; //no sense to perform this operation more than once per GOP

    for( std::set<AbstractMediaCacheEventReceiver*>::const_iterator
        it = m_eventReceivers.begin();
        it != m_eventReceivers.end();
        ++it )
    {
        (*it)->onKeyFrame( m_currentPacketTimestamp );
    }

    const quint64 maxTimestamp = (--m_packetsByTimestamp.end())->first;
    if( maxTimestamp - m_packetsByTimestamp.begin()->first > m_cacheSizeMillis*MICROS_PER_MS )
    {
        //determining left-most position of active blockings
        quint64 minReadingTimestamp = std::numeric_limits<quint64>::max();
        for( std::map<int, quint64>::const_iterator
            it = m_dataBlockings.begin();
            it != m_dataBlockings.end();
            ++it )
        {
            if( it->second < minReadingTimestamp )
                minReadingTimestamp = it->second;
        }

        //removing old packets up to key frame, but not futher any position of sequential reading
        PacketCotainerType::iterator lastItToRemove = m_packetsByTimestamp.lower_bound( std::min<>( minReadingTimestamp, maxTimestamp - m_cacheSizeMillis*MICROS_PER_MS ) );
        if( lastItToRemove != m_packetsByTimestamp.begin() )
            --lastItToRemove;
        //iterating to first key frame to the left from lastItToRemove
        for( ; lastItToRemove != m_packetsByTimestamp.begin() && !lastItToRemove->second.second; --lastItToRemove ) {}
        //updating cache size (in bytes)
        for( PacketCotainerType::iterator
            it = m_packetsByTimestamp.begin();
            it != lastItToRemove;
            ++it )
        {
            QnAbstractMediaDataPtr mediaPacket = it->second.first.dynamicCast<QnAbstractMediaData>();
            if( mediaPacket )
                m_cacheSizeInBytes -= mediaPacket->data.size();
        }
        m_packetsByTimestamp.erase( m_packetsByTimestamp.begin(), lastItToRemove );
    }
}

quint64 MediaStreamCache::startTimestamp() const
{
    QMutexLocker lk( &m_mutex );
    return m_packetsByTimestamp.empty() ? 0 : m_packetsByTimestamp.begin()->first;
}

quint64 MediaStreamCache::currentTimestamp() const
{
    QMutexLocker lk( &m_mutex );
    return m_packetsByTimestamp.empty() ? 0 : m_packetsByTimestamp.rbegin()->first;
}

//!Returns cached data size in micros
/*!
    Same as std::pair<qint64, qint64> p = availableDataRange()
    return p.second - p.first
*/
qint64 MediaStreamCache::duration() const
{
    QMutexLocker lk( &m_mutex );
    if( m_packetsByTimestamp.empty() )
        return 0;
    return m_packetsByTimestamp.rbegin()->first - m_packetsByTimestamp.begin()->first;
}

size_t MediaStreamCache::sizeInBytes() const
{
    QMutexLocker lk( &m_mutex );
    return m_cacheSizeInBytes;
}

int MediaStreamCache::getMaxBitrate() const
{
    QMutexLocker lk( &m_mutex );
    const qint64 durationUSec = m_packetsByTimestamp.empty() ? 0 : (m_packetsByTimestamp.rbegin()->first - m_packetsByTimestamp.begin()->first);
    if( durationUSec == 0 )
        return -1;
    return ((qint64)m_cacheSizeInBytes) * USEC_PER_SEC / durationUSec * CHAR_BIT;
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
    QMutexLocker lk( &m_mutex );

    if( m_packetsByTimestamp.empty() )
        return QnAbstractDataPacketPtr();

    PacketCotainerType::const_iterator it = m_packetsByTimestamp.lower_bound( desiredTimestamp );
    if( it == m_packetsByTimestamp.end() )
        --it;
    for( ;; )
    {
        if( it == m_packetsByTimestamp.end() )
            return QnAbstractDataPacketPtr();

        if( findKeyFrameOnly && !it->second.second )
        {
            //searching for I-frame
            --it;
            continue;
        }

        *foundTimestamp = it->first;
        return it->second.first;
    }
}

QnAbstractDataPacketPtr MediaStreamCache::getNextPacket( quint64 timestamp, quint64* const foundTimestamp ) const
{
    QMutexLocker lk( &m_mutex );

    PacketCotainerType::const_iterator it = m_packetsByTimestamp.upper_bound( timestamp );
    if( it == m_packetsByTimestamp.end() )
        return QnAbstractDataPacketPtr();

    *foundTimestamp = it->first;
    return it->second.first;
}

void MediaStreamCache::addEventReceiver( AbstractMediaCacheEventReceiver* const receiver )
{
    QMutexLocker lk( &m_mutex );
    m_eventReceivers.insert( receiver );
}

void MediaStreamCache::removeEventReceiver( AbstractMediaCacheEventReceiver* const receiver )
{
    QMutexLocker lk( &m_mutex );
    m_eventReceivers.erase( receiver );
}

int MediaStreamCache::blockData( quint64 timestamp )
{
    QMutexLocker lk( &m_mutex );

    //searching for a free blockingID
    int blockingID = 0;
    if( m_dataBlockings.empty() )
    {
        blockingID = 1;
    }
    else
    {
        //using only positive IDs
        for( std::map<int, quint64>::const_iterator
            it = m_dataBlockings.begin();
            it != m_dataBlockings.end();
            ++it )
        {
            if( it->first - blockingID > 1 )
                break;  //found gap in id
            blockingID = it->first;
        }
        ++blockingID;
    }

    if( !m_dataBlockings.insert( make_pair( blockingID, timestamp ) ).second )
    {
        Q_ASSERT( false );
    }

    return blockingID;
}

//!Updates position of blocking \a blockingID to \a timestampToMoveTo
void MediaStreamCache::moveBlocking( int blockingID, quint64 timestampToMoveTo )
{
    QMutexLocker lk( &m_mutex );

    std::map<int, quint64>::iterator it = m_dataBlockings.find( blockingID );
    if( it == m_dataBlockings.end() )
    {
        Q_ASSERT( false );
        return;
    }

    it->second = timestampToMoveTo;
}

//!Removed blocking \a blockingID
void MediaStreamCache::unblockData( int blockingID )
{
    QMutexLocker lk( &m_mutex );

    std::map<int, quint64>::iterator it = m_dataBlockings.find( blockingID );
    if( it == m_dataBlockings.end() )
    {
        Q_ASSERT( false );
        return;
    }
    m_dataBlockings.erase( it );
}
