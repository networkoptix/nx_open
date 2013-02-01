////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIASTREAMCACHE_H
#define MEDIASTREAMCACHE_H

#include <map>

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
    class SequentialReadContext
    {
    public:
        SequentialReadContext(
            const MediaStreamCache* cache,
            quint64 startTimestamp );

        //!If no next frame returns NULL
        QnAbstractDataPacketPtr getNextFrame();
        //!Returns timestamp of previous packet
        quint64 currentPos() const;

    private:
        const MediaStreamCache* m_cache;
        quint64 m_startTimestamp;
        bool m_firstFrame;
        quint64 m_currentTimestamp;
    };

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
    //!Returns packet witm min timestamp greater than \a timestamp
    QnAbstractDataPacketPtr getNextPacket( quint64 timestamp, quint64* const foundTimestamp ) const;

private:
    //!map<timestamp, pair<packet, key_flag> >
    typedef std::map<quint64, std::pair<QnAbstractDataPacketPtr, bool> > PacketCotainerType;

    unsigned int m_cacheSizeMillis;
    MediaIndex* const m_mediaIndex;
    PacketCotainerType m_packetsByTimestamp;
    mutable QMutex m_mutex;
    qint64 m_prevPacketSrcTimestamp;
    quint64 m_currentPacketTimestamp;
};

#endif  //MEDIASTREAMCACHE_H
