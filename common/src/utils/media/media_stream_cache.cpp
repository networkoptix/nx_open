////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "media_stream_cache.h"

#include <QMutexLocker>

#include "mediaindex.h"


using namespace std;

MediaStreamCache::SequentialReadContext::SequentialReadContext(
    const MediaStreamCache* cache,
    quint64 startTimestamp )
:
    m_cache( cache ),
    m_startTimestamp( startTimestamp ),
    m_firstFrame( true ),
    m_currentTimestamp( 0 )
{
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

    return m_cache->getNextPacket( m_currentTimestamp, &m_currentTimestamp );
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
    m_prevPacketSrcTimestamp( -1 ),
    m_currentPacketTimestamp( 0 )
{
}

//!Implementation of QnAbstractDataReceptor::canAcceptData
bool MediaStreamCache::canAcceptData() const
{
    return true;
}

static qint64 MAX_ALLOWED_TIMESTAMP_DIFF = 3600*1000*1000UL;  //1 hour

//!Implementation of QnAbstractDataReceptor::putData
void MediaStreamCache::putData( QnAbstractDataPacketPtr data )
{
    QMutexLocker lk( &m_mutex );

    QnAbstractMediaDataPtr mediaPacket = data.dynamicCast<QnAbstractMediaData>();
    const bool isKeyFrame = mediaPacket && (mediaPacket->flags & QnAbstractMediaData::MediaFlags_Key);
    if( m_packetsByTimestamp.empty() && !isKeyFrame )
        return; //cache data MUST start with key frame

    //calculating timestamp, because we cannot rely on data->timestamp, since it may contain discontinuity in case of reconnect to camera
    if( m_prevPacketSrcTimestamp == -1 )
        m_prevPacketSrcTimestamp = data->timestamp;

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

    m_packetsByTimestamp.insert( make_pair( m_currentPacketTimestamp, make_pair( data, isKeyFrame ) ) );
    if( m_packetsByTimestamp.rbegin()->first - m_packetsByTimestamp.begin()->first > m_cacheSizeMillis*1000 )
    {
        //TODO/IMPL removing old packets up to key frame
            //no sense to perform this operation more than once per GOP
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
    return m_packetsByTimestamp.rbegin()->first - m_packetsByTimestamp.begin()->first;
}

size_t MediaStreamCache::sizeInBytes() const
{
    //TODO/IMPL
    Q_ASSERT( false );
    return 0;
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
