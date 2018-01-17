////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIASTREAMCACHE_H
#define MEDIASTREAMCACHE_H

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>
#include <functional> /* For std::function. */

#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/subscription.h>
#include <nx/utils/thread/mutex.h>

namespace detail
{
    class MediaStreamCache;
}

//!Caches specified duration of media stream for later use
/*!
    \note Class is thread-safe (concurrent threads can read and write to class instance)
    \note Always tries to perform so that first cache frame is an I-frame
*/
class MediaStreamCache
:
    public QnAbstractMediaDataReceptor
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
        std::weak_ptr<detail::MediaStreamCache> m_sharedCache;
        quint64 m_startTimestamp;
        bool m_firstFrame;
        //!timestamp of previous given frame
        quint64 m_currentTimestamp;
        int m_blockingID;
    };

    /*!
        \param cacheSizeMillis Data older than, \a last_frame_timestamp - \a cacheSizeMillis is dropped
    */
    MediaStreamCache(
        unsigned int cacheSizeMillis,
        unsigned int maxCacheSizeMillis);
    virtual ~MediaStreamCache();

    //!Implementation of QnAbstractMediaDataReceptor::canAcceptData
    virtual bool canAcceptData() const override;
    //!Implementation of QnAbstractMediaDataReceptor::putData
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

    nx::utils::Subscription<quint64 /*frameTimestampUsec*/>& keyFrameFoundSubscription();
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
protected:
    virtual bool needConfigureProvider() const override { return false; }
private:
    std::shared_ptr<detail::MediaStreamCache> m_sharedImpl;
};

#endif // ENABLE_DATA_PROVIDERS

#endif  //MEDIASTREAMCACHE_H
