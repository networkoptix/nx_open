/**********************************************************
* Aug 24, 2015
* a.kolesnikov
***********************************************************/

#ifdef ENABLE_DATA_PROVIDERS

#include "media_stream_cache_detail.h"
#include "utils/media/media_stream_cache.h"

#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>

namespace detail
{

static qint64 MAX_ALLOWED_TIMESTAMP_DIFF = 5 * 1000 * 1000UL;  //5 seconds
static const int USEC_PER_MS = 1000;
static const int USEC_PER_SEC = 1000 * 1000;

MediaStreamCache::MediaStreamCache(
    unsigned int desiredCacheSizeMillis,
    unsigned int maxCacheSizeMillis)
:
    m_cacheSizeUsec( ((qint64)desiredCacheSizeMillis)*USEC_PER_MS),
    m_maxCacheSizeUsec( ((qint64)maxCacheSizeMillis)*USEC_PER_MS ),
    m_mutex( QnMutex::Recursive ),   //TODO #ak get rid of Recursive mutex
    m_prevPacketSrcTimestamp( -1 ),
    m_cacheSizeInBytes( 0 )
{
    m_inactivityTimer.restart();
}

//! Implementation of QnAbstractMediaDataReceptor::canAcceptData
bool MediaStreamCache::canAcceptData() const
{
    return true;
}

//! Implementation of QnAbstractMediaDataReceptor::putData
void MediaStreamCache::putData(const QnAbstractDataPacketPtr& data)
{
    std::vector<std::function<void()>> eventsToDeliver;
    auto eventsToDeliverGuard = nx::utils::makeScopeGuard(
        [&eventsToDeliver, this]()
        {
            for (auto& deliverEvent: eventsToDeliver)
                deliverEvent();
        });

    QnMutexLocker lk(&m_mutex);

    const QnAbstractMediaData* mediaPacket = dynamic_cast<QnAbstractMediaData*>(data.get());
    NX_CRITICAL(mediaPacket);

    const bool isKeyFrame = mediaPacket->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey);
    NX_VERBOSE(this, "Got frame [%1], isKeyFrame [%2], channel [%3]",
        data->timestamp, isKeyFrame, mediaPacket->channelNumber);

    if (m_packetsByTimestamp.empty() && !isKeyFrame)
        return; // Cache data MUST start with key frame.

    if (m_prevPacketSrcTimestamp == -1)
        m_prevPacketSrcTimestamp = data->timestamp;

    if (qAbs(data->timestamp - m_prevPacketSrcTimestamp) > MAX_ALLOWED_TIMESTAMP_DIFF)
        eventsToDeliver.push_back([this]() { m_onDiscontinue.notify(); });

    m_prevPacketSrcTimestamp = data->timestamp;

    // Searching position to insert to. In most cases (or in every case in case of h.264@baseline)
    // insertion is done to the end.
    PacketContainerType::iterator posToInsert = m_packetsByTimestamp.end();
    for (auto rIt = m_packetsByTimestamp.rbegin(); rIt != m_packetsByTimestamp.rend(); ++rIt)
    {
        if (rIt->timestamp <= static_cast<quint64>(data->timestamp))
        {
            posToInsert = rIt.base();
            break;
        }
    }

    m_packetsByTimestamp.insert(
        posToInsert,
        {quint64(data->timestamp), data, isKeyFrame, int(mediaPacket->channelNumber)});

    m_cacheSizeInBytes += mediaPacket->dataSize();
    clearCacheIfNeeded(&lk);

    if(!isKeyFrame)
        return; // No sense to perform this operation more than once per GOP.

#ifdef DEBUG_OUTPUT
    NX_VERBOSE(this, "Media cache size %1 sec, %2 bytes, m_packetsByTimestamp.size() = %3",
        (m_packetsByTimestamp.empty()
            ? 0
            : (m_packetsByTimestamp.back().timestamp - m_packetsByTimestamp.front().timestamp))
            / 1000000,
        m_cacheSizeInBytes,
        m_packetsByTimestamp.size());
    static int malloc_statsCounter = 0;
    ++malloc_statsCounter;
    if (malloc_statsCounter == 10)
    {
        malloc_stats();
        malloc_statsCounter = 0;
    }
#endif
    
    eventsToDeliver.push_back(
        [this, timestamp = data->timestamp]() { m_onKeyFrame.notify(timestamp); });
}

void MediaStreamCache::clearCacheIfNeeded(QnMutexLockerBase* const /*lk*/)
{
    const quint64 maxTimestamp = m_packetsByTimestamp.back().timestamp;
    if (maxTimestamp - m_packetsByTimestamp.front().timestamp <= quint64(m_cacheSizeUsec))
        return;

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

    //putting a hard limit on cache size by time and packet count
    const qint64 currentCacheSizeUsec = maxTimestamp - minReadingTimestamp;
    if (currentCacheSizeUsec > m_maxCacheSizeUsec)
        minReadingTimestamp = maxTimestamp - m_maxCacheSizeUsec;

    //removing old packets up to key frame, but not futher any position of sequential reading
    PacketContainerType::iterator lastItToRemove = std::lower_bound(
        m_packetsByTimestamp.begin(),
        m_packetsByTimestamp.end(),
        std::min<>(minReadingTimestamp, maxTimestamp - m_cacheSizeUsec),
        []( const MediaStreamCache::MediaPacketContext& one, quint64 two ) -> bool {
            return one.timestamp < two;
        } );
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
        const QnAbstractMediaData* mediaPacket = dynamic_cast<QnAbstractMediaData*>(it->packet.get());
        if( mediaPacket )
            m_cacheSizeInBytes -= mediaPacket->dataSize();
    }
    m_packetsByTimestamp.erase( m_packetsByTimestamp.begin(), lastItToRemove );
}

void MediaStreamCache::clear()
{
    QnMutexLocker lk(&m_mutex);

    m_prevPacketSrcTimestamp = -1;
    m_cacheSizeInBytes = 0;
    m_packetsByTimestamp.clear();
}

quint64 MediaStreamCache::startTimestamp() const
{
    QnMutexLocker lk(&m_mutex);
    return m_packetsByTimestamp.empty() ? 0 : m_packetsByTimestamp.begin()->timestamp;
}

quint64 MediaStreamCache::currentTimestamp() const
{
    QnMutexLocker lk(&m_mutex);
    return m_packetsByTimestamp.empty() ? 0 : m_packetsByTimestamp.rbegin()->timestamp;
}

//!Returns cached data size in micros
/*!
    Same as std::pair<qint64, qint64> p = availableDataRange()
    return p.second - p.first
*/
qint64 MediaStreamCache::duration() const
{
    QnMutexLocker lk(&m_mutex);
    if( m_packetsByTimestamp.empty() )
        return 0;
    return m_packetsByTimestamp.rbegin()->timestamp - m_packetsByTimestamp.begin()->timestamp;
}

size_t MediaStreamCache::sizeInBytes() const
{
    QnMutexLocker lk(&m_mutex);
    return m_cacheSizeInBytes;
}

int MediaStreamCache::getMaxBitrate() const
{
    QnMutexLocker lk(&m_mutex);
    const qint64 durationUSec = m_packetsByTimestamp.empty() ? 0 : (m_packetsByTimestamp.rbegin()->timestamp - m_packetsByTimestamp.begin()->timestamp);
    if( durationUSec == 0 )
        return -1;
    //using int64 for intermediate result to avoid int overflow while multiplying by USEC_PER_SEC.
    //  Multiplication is before division to keep accuracy
    return ((qint64)m_cacheSizeInBytes) * USEC_PER_SEC / durationUSec * CHAR_BIT;
}

/**
    Returns packet with timestamp == \a timestamp or packet with closest (from the left) timestamp.
    In other words, this methods performs lower_bound in timestamp-sorted (using 'greater'
    comparision) array of data packets.
*/
QnAbstractDataPacketPtr MediaStreamCache::findByTimestamp(quint64 desiredTimestamp,
    bool findKeyFrameOnly, quint64* const foundTimestamp, int channelNumber) const
{
    QnMutexLocker lk(&m_mutex);

    m_inactivityTimer.restart();
    const auto result = std::find_if(m_packetsByTimestamp.crbegin(), m_packetsByTimestamp.crend(),
        [channelNumber, findKeyFrameOnly, desiredTimestamp](const auto& value)
        {
            if (value.timestamp > desiredTimestamp)
                return false;
            if (value.channelNumber != channelNumber)
                return false;
            if (!findKeyFrameOnly)
                return true;
            if (value.isKeyFrame)
                return true;
            return false;
        });

    if (result == m_packetsByTimestamp.crend())
        return {};
    using namespace std::chrono;
    if (desiredTimestamp - result->timestamp > duration_cast<microseconds>(kMaxTimestampDeviation).count())
        return {};

    *foundTimestamp = result->timestamp;
    return result->packet;
}

QnAbstractDataPacketPtr MediaStreamCache::getNextPacket(
    quint64 timestamp, quint64* const foundTimestamp, int channelNumber) const
{
    QnMutexLocker lk(&m_mutex);

    // Next frame after the one to start search from.
    PacketContainerType::const_iterator it = std::upper_bound(
        m_packetsByTimestamp.cbegin(), m_packetsByTimestamp.cend(), timestamp,
        [](quint64 bound, const auto& frame) { return bound < frame.timestamp; });

    it = std::find_if(it, m_packetsByTimestamp.cend(),
        [channelNumber](const auto& value) { return value.channelNumber == channelNumber; });

    if (it == m_packetsByTimestamp.cend())
        return {};

    *foundTimestamp = it->timestamp;
    return it->packet;
}

nx::utils::Subscription<quint64 /*currentPacketTimestampUSec*/>& MediaStreamCache::keyFrameFoundSubscription()
{
    return m_onKeyFrame;
}

nx::utils::Subscription<>& MediaStreamCache::streamTimeDiscontinuityFoundSubscription()
{
    return m_onDiscontinue;
}

int MediaStreamCache::blockData( quint64 timestamp )
{
    QnMutexLocker lk(&m_mutex);

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

    if( !m_dataBlockings.emplace( blockingID, timestamp ).second )
    {
        NX_ASSERT( false );
    }

    return blockingID;
}

//!Updates position of blocking \a blockingID to \a timestampToMoveTo
void MediaStreamCache::moveBlocking( int blockingID, quint64 timestampToMoveTo )
{
    QnMutexLocker lk(&m_mutex);

    std::map<int, quint64>::iterator it = m_dataBlockings.find( blockingID );
    if( it == m_dataBlockings.end() )
    {
        NX_ASSERT( false );
        return;
    }

    it->second = timestampToMoveTo;
}

//!Removed blocking \a blockingID
void MediaStreamCache::unblockData( int blockingID )
{
    QnMutexLocker lk(&m_mutex);

    std::map<int, quint64>::iterator it = m_dataBlockings.find( blockingID );
    if( it == m_dataBlockings.end() )
    {
        NX_ASSERT( false );
        return;
    }
    m_dataBlockings.erase( it );
}

qint64 MediaStreamCache::inactivityPeriod() const
{
    QnMutexLocker lk(&m_mutex);
    return m_inactivityTimer.elapsed();
}

}

#endif
