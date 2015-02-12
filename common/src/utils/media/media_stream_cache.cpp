////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "media_stream_cache.h"

#include <cstdlib>
#include <algorithm>

//#define DEBUG_OUTPUT
#ifdef DEBUG_OUTPUT
#include <malloc.h>
#endif


using namespace std;

static const int USEC_PER_SEC = 1000*1000;

static bool compare1_lower_bound( const MediaStreamCache::MediaPacketContext& one, quint64 two )
{
    return one.timestamp < two;
}

static bool compare1_upper_bound( quint64 one, const MediaStreamCache::MediaPacketContext& two )
{
    return one < two.timestamp;
}

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


MediaStreamCache::MediaStreamCache( unsigned int cacheSizeMillis )
:
    m_cacheSizeMillis( cacheSizeMillis ),
    m_mutex( QnMutex::Recursive ),   //TODO #ak get rid of Recursive mutex
    m_prevPacketSrcTimestamp( -1 ),
    m_currentPacketTimestamp( 0 ),
    m_cacheSizeInBytes( 0 ),
    m_prevGivenEventReceiverID( 0 )
{
    m_inactivityTimer.restart();
}

//!Implementation of QnAbstractDataReceptor::canAcceptData
bool MediaStreamCache::canAcceptData() const
{
    return true;
}

static qint64 MAX_ALLOWED_TIMESTAMP_DIFF = 5*1000*1000UL;  //5 seconds
static const int MICROS_PER_MS = 1000;

//!Implementation of QnAbstractDataReceptor::putData
void MediaStreamCache::putData( const QnAbstractDataPacketPtr& data )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    const QnAbstractMediaData* mediaPacket = dynamic_cast<QnAbstractMediaData*>(data.data());
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

    //searching position to insert to. in most cases (or in every case in case of h.264@baseline) insertion is done to the end
    PacketContainerType::iterator posToInsert = m_packetsByTimestamp.end();
    for( PacketContainerType::reverse_iterator
        rIt = m_packetsByTimestamp.rbegin();
        rIt != m_packetsByTimestamp.rend();
        ++rIt )
    {
        if( rIt->timestamp <= m_currentPacketTimestamp )
        {
            posToInsert = rIt.base();
            break;
        }
    }

    m_packetsByTimestamp.insert( posToInsert, MediaPacketContext( m_currentPacketTimestamp, data, isKeyFrame ) );
    if( mediaPacket )
        m_cacheSizeInBytes += mediaPacket->dataSize();

    if( !isKeyFrame )
        return; //no sense to perform this operation more than once per GOP

#ifdef DEBUG_OUTPUT
    std::cout<<"Media cache size "<<(m_packetsByTimestamp.empty() ? 0 : (m_packetsByTimestamp.back().timestamp - m_packetsByTimestamp.front().timestamp)) / 1000000<<" sec"
        ", "<<m_cacheSizeInBytes<<" bytes"<<
        ", m_packetsByTimestamp.size() = "<<m_packetsByTimestamp.size()<<
        //", QnAbstractMediaData count "<<QnAbstractMediaData_instanceCount.load()<<
        //", QnByteArray_bytesAllocated "<<QnByteArray_bytesAllocated.load()<<
        std::endl;

    static int malloc_statsCounter = 0;
    ++malloc_statsCounter;
    if( malloc_statsCounter == 10 )
    {
        malloc_stats();
        malloc_statsCounter = 0;
    }
#endif

    for( auto eventReceiver: m_eventReceivers )
        eventReceiver.second( m_currentPacketTimestamp );
    const quint64 maxTimestamp = m_packetsByTimestamp.back().timestamp;
    if( maxTimestamp - m_packetsByTimestamp.front().timestamp > m_cacheSizeMillis*MICROS_PER_MS )
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
        //PacketContainerType::iterator lastItToRemove = m_packetsByTimestamp.lower_bound( std::min<>( minReadingTimestamp, maxTimestamp - m_cacheSizeMillis*MICROS_PER_MS ) );
        PacketContainerType::iterator lastItToRemove = std::lower_bound(
            m_packetsByTimestamp.begin(),
            m_packetsByTimestamp.end(),
            std::min<>( minReadingTimestamp, maxTimestamp - m_cacheSizeMillis*MICROS_PER_MS ),
            compare1_lower_bound );
        if( lastItToRemove != m_packetsByTimestamp.begin() )
            --lastItToRemove;
        //iterating to first key frame to the left from lastItToRemove
        for( ; lastItToRemove != m_packetsByTimestamp.begin() && !lastItToRemove->isKeyFrame; --lastItToRemove ) {}
        //updating cache size (in bytes)
        for( PacketContainerType::const_iterator
            it = m_packetsByTimestamp.cbegin();
            it != lastItToRemove;
            ++it )
        {
            const QnAbstractMediaData* mediaPacket = dynamic_cast<QnAbstractMediaData*>(it->packet.data());
            if( mediaPacket )
                m_cacheSizeInBytes -= mediaPacket->dataSize();
        }
        m_packetsByTimestamp.erase( m_packetsByTimestamp.begin(), lastItToRemove );
    }
}

void MediaStreamCache::clear()
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    m_prevPacketSrcTimestamp = -1;
    m_currentPacketTimestamp = 0;
    m_cacheSizeInBytes = 0;
    m_packetsByTimestamp.clear();
}

quint64 MediaStreamCache::startTimestamp() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return m_packetsByTimestamp.empty() ? 0 : m_packetsByTimestamp.begin()->timestamp;
}

quint64 MediaStreamCache::currentTimestamp() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return m_packetsByTimestamp.empty() ? 0 : m_packetsByTimestamp.rbegin()->timestamp;
}

//!Returns cached data size in micros
/*!
    Same as std::pair<qint64, qint64> p = availableDataRange()
    return p.second - p.first
*/
qint64 MediaStreamCache::duration() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    if( m_packetsByTimestamp.empty() )
        return 0;
    return m_packetsByTimestamp.rbegin()->timestamp - m_packetsByTimestamp.begin()->timestamp;
}

size_t MediaStreamCache::sizeInBytes() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return m_cacheSizeInBytes;
}

int MediaStreamCache::getMaxBitrate() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    const qint64 durationUSec = m_packetsByTimestamp.empty() ? 0 : (m_packetsByTimestamp.rbegin()->timestamp - m_packetsByTimestamp.begin()->timestamp);
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
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    m_inactivityTimer.restart();

    if( m_packetsByTimestamp.empty() )
        return QnAbstractDataPacketPtr();

    PacketContainerType::const_iterator it = std::lower_bound( m_packetsByTimestamp.cbegin(), m_packetsByTimestamp.cend(), desiredTimestamp, compare1_lower_bound );
    if( it == m_packetsByTimestamp.cend() )
        --it;
    for( ;; )
    {
        if( it == m_packetsByTimestamp.cend() )
            return QnAbstractDataPacketPtr();

        if( findKeyFrameOnly && !it->isKeyFrame )
        {
            //searching for I-frame
            --it;
            continue;
        }

        *foundTimestamp = it->timestamp;
        return it->packet;
    }
}

QnAbstractDataPacketPtr MediaStreamCache::getNextPacket( quint64 timestamp, quint64* const foundTimestamp ) const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    PacketContainerType::const_iterator it = std::upper_bound( m_packetsByTimestamp.cbegin(), m_packetsByTimestamp.cend(), timestamp, compare1_upper_bound );
    if( it == m_packetsByTimestamp.end() )
        return QnAbstractDataPacketPtr();

    *foundTimestamp = it->timestamp;
    return it->packet;
}

int MediaStreamCache::addKeyFrameEventReceiver( const std::function<void (quint64)>& keyFrameEventReceiver )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    m_eventReceivers.insert( std::make_pair( ++m_prevGivenEventReceiverID, keyFrameEventReceiver ) );
    return m_prevGivenEventReceiverID;
}

void MediaStreamCache::removeKeyFrameEventReceiver( int receiverID )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    m_eventReceivers.erase( receiverID );
}

int MediaStreamCache::blockData( quint64 timestamp )
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

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
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

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
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );

    std::map<int, quint64>::iterator it = m_dataBlockings.find( blockingID );
    if( it == m_dataBlockings.end() )
    {
        Q_ASSERT( false );
        return;
    }
    m_dataBlockings.erase( it );
}

size_t MediaStreamCache::inactivityPeriod() const
{
    SCOPED_MUTEX_LOCK( lk,  &m_mutex );
    return m_inactivityTimer.elapsed();
}
