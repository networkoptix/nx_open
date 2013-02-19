////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIASTREAMCACHE_H
#define MEDIASTREAMCACHE_H

#include <map>
#include <vector>

#include <QMutex>

#include <core/dataconsumer/abstract_data_receptor.h>


class MediaIndex;

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
            const MediaStreamCache* cache,
            quint64 startTimestamp );
        ~SequentialReadContext();

        //!If no next frame returns NULL
        QnAbstractDataPacketPtr getNextFrame();
        //!Returns timestamp of previous packet
        quint64 currentPos() const;

    private:
        const MediaStreamCache* m_cache;
        quint64 m_startTimestamp;
        bool m_firstFrame;
        //!timestamp of previous given frame
        quint64 m_currentTimestamp;
    };

    /*!
        \param cacheSizeMillis Data older than, \a last_frame_timestamp - \a cacheSizeMillis is dropped
    */
    MediaStreamCache(
        unsigned int cacheSizeMillis,
        MediaIndex* const mediaIndex );

    //!Implementation of QnAbstractDataReceptor::canAcceptData
    virtual bool canAcceptData() const override;
    //!Implementation of QnAbstractDataReceptor::putData
    virtual void putData( QnAbstractDataPacketPtr data ) override;

    quint64 startTimestamp() const;
    quint64 currentTimestamp() const;
    //!Returns cached data size in micros
    /*!
        Same as std::pair<qint64, qint64> p = availableDataRange()
        return p.second - p.first
    */
    qint64 duration() const;
    size_t sizeInBytes() const;

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

private:
    //!map<timestamp, pair<packet, key_flag> >
    typedef std::map<quint64, std::pair<QnAbstractDataPacketPtr, bool> > PacketCotainerType;

    unsigned int m_cacheSizeMillis;
    MediaIndex* const m_mediaIndex;
    PacketCotainerType m_packetsByTimestamp;
    mutable QMutex m_mutex;
    qint64 m_prevPacketSrcTimestamp;
    //!In micros
    quint64 m_currentPacketTimestamp;
    mutable std::vector<SequentialReadContext*> m_activeReadContexts;

    void sequentialReadingStarted( SequentialReadContext* const readCtx ) const;
    void sequentialReadingFinished( SequentialReadContext* const readCtx ) const;
};

#endif  //MEDIASTREAMCACHE_H
