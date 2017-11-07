/**********************************************************
* Aug 24, 2015
* a.kolesnikov
***********************************************************/

#ifndef MEDIA_STREAM_CACHE_DETAIL_H
#define MEDIA_STREAM_CACHE_DETAIL_H

#ifdef ENABLE_DATA_PROVIDERS

#include <deque>
#include <map>
#include <set>

#include <QtCore/QElapsedTimer>

#include <nx/streaming/abstract_data_packet.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

namespace detail {

class MediaStreamCache
{
public:
    /*!
        \param desiredCacheSizeMillis Data older than, \a last_frame_timestamp - \a cacheSizeMillis 
            is dropped unless data is blocked by \a MediaStreamCache::blockData call
        \param maxCacheSizeMillis if cache size increases this value, data is dropped despite existing blockings
    */
    MediaStreamCache(
        unsigned int desiredCacheSizeMillis,
        unsigned int maxCacheSizeMillis);

    //!Implementation of QnAbstractMediaDataReceptor::canAcceptData
    bool canAcceptData() const;
    //!Implementation of QnAbstractMediaDataReceptor::putData
    void putData( const QnAbstractDataPacketPtr& data );

    void clear();

    quint64 startTimestamp() const;
    quint64 currentTimestamp() const;
    //!Returns cached data size in micros
    /*!
        Same as std::pair<qint64, qint64> p = availableDataRange()
        return p.second - p.first
    */
    qint64 duration() const;
    size_t sizeInBytes() const;
    /*!
        \return -1, if bitrate unknown
    */
    int getMaxBitrate() const;

    //!Returns packet with timestamp == \a desiredTimestamp or packet with closest (from the left) timestamp
    /*!
        In other words, this methods performs lower_bound in timestamp-sorted (using 'greater' comparision) array of data packets
        \param findKeyFrameOnly If true, searches for key frame only
    */
    QnAbstractDataPacketPtr findByTimestamp(
        quint64 desiredTimestamp,
        bool findKeyFrameOnly,
        quint64* const foundTimestamp ) const;
    //!Returns packet with min timestamp greater than \a timestamp
    QnAbstractDataPacketPtr getNextPacket( quint64 timestamp, quint64* const foundTimestamp ) const;

    nx::utils::Subscription<quint64 /*currentPacketTimestampUSec*/>& keyFrameFoundSubscription();
    nx::utils::Subscription<>& streamTimeDiscontinuityFoundSubscription();

    //!Prevents data starting with \a timestamp from removal
    /*!
        \return ID of created blocking. -1 is reserved value and is never returned
    */
    int blockData( quint64 timestamp );
    //!Updates position of blocking \a blockingID to \a timestampToMoveTo
    void moveBlocking( int blockingID, quint64 timestampToMoveTo );
    //!Removed blocking \a blockingID
    void unblockData( int blockingID );

    //!Time (millis) from last usage of this object
    qint64 inactivityPeriod() const;

private:
    struct MediaPacketContext
    {
        quint64 timestamp;
        QnAbstractDataPacketPtr packet;
        bool isKeyFrame;

        MediaPacketContext()
            :
            timestamp( 0 ),
            isKeyFrame( false )
        {
        }

        MediaPacketContext(
            quint64 _timestamp,
            QnAbstractDataPacketPtr _packet,
            bool _isKeyFrame )
            :
            timestamp( _timestamp ),
            packet( _packet ),
            isKeyFrame( _isKeyFrame )
        {
        }
    };

    //!map<timestamp, pair<packet, key_flag> >
    typedef std::deque<MediaPacketContext> PacketContainerType;

    const qint64 m_cacheSizeUsec;
    const qint64 m_maxCacheSizeUsec;
    PacketContainerType m_packetsByTimestamp;
    mutable QnMutex m_mutex;
    //!In micros
    qint64 m_prevPacketSrcTimestamp;
    size_t m_cacheSizeInBytes;
    std::map<int, quint64> m_dataBlockings;
    mutable QElapsedTimer m_inactivityTimer;
    nx::utils::Subscription<quint64 /*currentPacketTimestampUSec*/> m_onKeyFrame;
    nx::utils::Subscription<> m_onDiscontinue;

    void clearCacheIfNeeded(QnMutexLockerBase* const lk);
};

}

#endif

#endif  //MEDIA_STREAM_CACHE_DETAIL_H
