////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "media_stream_cache.h"

#include <algorithm>

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
    m_currentTimestamp( startTimestamp )
{
    cache->sequentialReadingStarted( this );
}

MediaStreamCache::SequentialReadContext::~SequentialReadContext()
{
    m_cache->sequentialReadingFinished( this );
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
static const int MICROS_PER_MS = 1000;

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

    if( !isKeyFrame )
        return;

    const quint64 maxTimestamp = (--m_packetsByTimestamp.end())->first;
    if( maxTimestamp - m_packetsByTimestamp.begin()->first > m_cacheSizeMillis*MICROS_PER_MS )
    {
        //TODO/IMPL/HLS removing old packets up to key frame, but not futher any position of sequential reading
        PacketCotainerType::iterator lastItToRemove = m_packetsByTimestamp.lower_bound( maxTimestamp - m_cacheSizeMillis*MICROS_PER_MS );
        if( lastItToRemove != m_packetsByTimestamp.begin() )
            --lastItToRemove ;
        m_packetsByTimestamp.erase( m_packetsByTimestamp.begin(), lastItToRemove  );
            //TODO/IMPL/HLS no sense to perform this operation more than once per GOP
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

void MediaStreamCache::sequentialReadingStarted( SequentialReadContext* const readCtx ) const
{
    QMutexLocker lk( &m_mutex );
    m_activeReadContexts.push_back( readCtx );
}

void MediaStreamCache::sequentialReadingFinished( SequentialReadContext* const readCtx ) const
{
    QMutexLocker lk( &m_mutex );
    m_activeReadContexts.erase( std::remove( m_activeReadContexts.begin(), m_activeReadContexts.end(), readCtx ), m_activeReadContexts.end() );
}
