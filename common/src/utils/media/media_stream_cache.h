////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIASTREAMCACHE_H
#define MEDIASTREAMCACHE_H

#include <core/dataconsumer/abstract_data_receptor.h>


class MediaIndex;

//!Caches specified duration of media stream for later use
/*!
    \note Class is thread-safe (concurrent threads can read and write to class instance)
*/
class MediaStreamCache
:
    public QnAbstractDataReceptor
{
public:
    MediaStreamCache(
        unsigned int cacheSizeMillis,
        MediaIndex* const mediaIndex );

    //!Implementation of QnAbstractDataReceptor::canAcceptData
    virtual bool canAcceptData() const override;
    //!Implementation of QnAbstractDataReceptor::putData
    virtual void putData( QnAbstractDataPacketPtr data ) override;

    QDateTime startTime() const;
    QDateTime endTime() const;
    /*!
        \return pair<start timestamp, stop timestamp>
    */
    std::pair<qint64, qint64> availableDataRange() const;
    //!Returns cached data size in micros
    /*!
        Same as std::pair<qint64, qint64> p = availableDataRange()
        return p.second - p.first
    */
    qint64 duration() const;
    size_t sizeInBytes() const;

    //!Returns packet with timestamp == \a timestamp or packet with closest (from the left) timestamp
    /*!
        In other words, this methods performs lower_bound in timestamp-sorted (using 'greater' comparision) array of data packets
    */
    QnAbstractDataPacketPtr findByTimestamp( qint64 timestamp ) const;

private:
    unsigned int m_cacheSizeMillis;
    MediaIndex* const m_mediaIndex;
};

#endif  //MEDIASTREAMCACHE_H
