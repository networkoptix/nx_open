////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "media_stream_cache.h"


MediaStreamCache::MediaStreamCache(
    unsigned int cacheSizeMillis,
    MediaIndex* const mediaIndex )
:
    m_cacheSizeMillis( cacheSizeMillis ),
    m_mediaIndex( mediaIndex )
{
    Q_ASSERT( false );
}

//!Implementation of QnAbstractDataReceptor::canAcceptData
bool MediaStreamCache::canAcceptData() const
{
    //TODO/IMPL
    return false;
}

//!Implementation of QnAbstractDataReceptor::putData
void MediaStreamCache::putData( QnAbstractDataPacketPtr data )
{
    //TODO/IMPL
}

QDateTime MediaStreamCache::startTime() const
{
    //TODO/IMPL
    return QDateTime();
}

QDateTime MediaStreamCache::endTime() const
{
    //TODO/IMPL
    return QDateTime();
}

/*!
    \return pair<start timestamp, stop timestamp>
*/
std::pair<qint64, qint64> MediaStreamCache::availableDataRange() const
{
    //TODO/IMPL
    return std::pair<qint64, qint64>( 0, 0 );
}

//!Returns cached data size in micros
/*!
    Same as std::pair<qint64, qint64> p = availableDataRange()
    return p.second - p.first
*/
qint64 MediaStreamCache::duration() const
{
    //TODO/IMPL
    return 0;
}

size_t MediaStreamCache::sizeInBytes() const
{
    //TODO/IMPL
    return 0;
}

//!Returns packet with timestamp == \a timestamp or packet with closest (from the left) timestamp
/*!
    In other words, this methods performs lower_bound in timestamp-sorted (using 'greater' comparision) array of data packets
*/
QnAbstractDataPacketPtr MediaStreamCache::findByTimestamp( qint64 timestamp ) const
{
    //TODO/IMPL
    return QnAbstractDataPacketPtr();
}
