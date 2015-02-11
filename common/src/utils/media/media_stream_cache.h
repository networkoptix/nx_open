////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIASTREAMCACHE_H
#define MEDIASTREAMCACHE_H

#include <deque>
#include <map>
#include <set>
#include <vector>
#include <functional> /* For std::function. */

#include <QtCore/QElapsedTimer>
#include <utils/common/mutex.h>

#include <core/dataconsumer/abstract_data_receptor.h>



//!Caches specified duration of media stream for later use
/*!
    \note Class is thread-safe (concurrent threads can read and write to class instance)
    \note Always tries to perform so that first cache frame is an I-frame
*/
class MediaStreamCache
:
    public QnAbstractDataReceptor
{
public:
    /*!
        By using this class, one can be sure that old data will not be removed until it has been read
        \note Class is not thread-safe
    */
    class SequentialReadContext
    {
    public:
        SequentialReadContext(
            MediaStreamCache* cache,
            quint64 startTimestamp );
        ~SequentialReadContext();

        //!If no next frame returns NULL
        QnAbstractDataPacketPtr getNextFrame();
        //!Returns timestamp of previous packet
        quint64 currentPos() const;

    private:
        MediaStreamCache* m_cache;
        quint64 m_startTimestamp;
        bool m_firstFrame;
        //!timestamp of previous given frame
        quint64 m_currentTimestamp;
        int m_blockingID;
    };

    /*!
        \param cacheSizeMillis Data older than, \a last_frame_timestamp - \a cacheSizeMillis is dropped
    */
    MediaStreamCache( unsigned int cacheSizeMillis );

    //!Implementation of QnAbstractDataReceptor::canAcceptData
    virtual bool canAcceptData() const override;
    //!Implementation of QnAbstractDataReceptor::putData
    virtual void putData( const QnAbstractDataPacketPtr& data ) override;

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
    int addKeyFrameEventReceiver( const std::function<void (quint64)>& keyFrameEventReceiver );
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
    size_t inactivityPeriod() const;

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

private:
    //!map<timestamp, pair<packet, key_flag> >
    typedef std::deque<MediaPacketContext> PacketContainerType;

    unsigned int m_cacheSizeMillis;
    PacketContainerType m_packetsByTimestamp;
    mutable QnMutex m_mutex;
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

#endif  //MEDIASTREAMCACHE_H
