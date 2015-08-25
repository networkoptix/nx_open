/**********************************************************
* Aug 24, 2015
* a.kolesnikov
***********************************************************/

#ifndef MEDIA_STREAM_CACHE_DETAIL_H
#define MEDIA_STREAM_CACHE_DETAIL_H

#include <deque>
#include <map>

#include <QtCore/QElapsedTimer>

#include <core/datapacket/abstract_data_packet.h>


namespace detail {

class MediaStreamCache
{
public:
    /*!
        \param cacheSizeMillis Data older than, \a last_frame_timestamp - \a cacheSizeMillis is dropped
    */
    MediaStreamCache( unsigned int cacheSizeMillis );

    //!Implementation of QnAbstractDataReceptor::canAcceptData
    bool canAcceptData() const;
    //!Implementation of QnAbstractDataReceptor::putData
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

    /*!
        \return id of event receiver
    */
    int addKeyFrameEventReceiver( std::function<void (quint64)> keyFrameEventReceiver );
    /*!
        \param receiverID id received from \a MediaStreamCache::addKeyFrameEventReceiver
    */
    void removeKeyFrameEventReceiver( int receiverID );

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

    unsigned int m_cacheSizeMillis;
    PacketContainerType m_packetsByTimestamp;
    mutable QMutex m_mutex;
    qint64 m_prevPacketSrcTimestamp;
    //!In micros
    quint64 m_currentPacketTimestamp;
    size_t m_cacheSizeInBytes;
    //!map<event receiver id, function>
    std::map<int, std::function<void (quint64)> > m_eventReceivers;
    int m_prevGivenEventReceiverID;
    std::map<int, quint64> m_dataBlockings;
    mutable QElapsedTimer m_inactivityTimer;
};

}

#endif  //MEDIA_STREAM_CACHE_DETAIL_H
