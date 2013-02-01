////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_cache_key.h"


StreamingChunkCacheKey::StreamingChunkCacheKey()
{
    Q_ASSERT( false );
}

/*!
    \param channel channel number or -1 for all channels
    \param containerFormat E.g., ts for mpeg2/ts
    \param startTimestamp
    \param endTimestamp
    \param auxiliaryParams
*/
StreamingChunkCacheKey::StreamingChunkCacheKey(
    const QString& uniqueResourceID,
    int channel,
    const QString& containerFormat,
    quint64 startTimestamp,
    quint64 duration,
    const std::multimap<QString, QString>& auxiliaryParams )
{
}

//!data source (camera id, stream id)
QString StreamingChunkCacheKey::srcResourceUniqueID() const
{
    //TODO/IMPL
    return QString();
}

unsigned int StreamingChunkCacheKey::channel() const
{
    //TODO/IMPL
    return 0;
}

quint64 StreamingChunkCacheKey::startTimestamp() const
{
    //TODO/IMPL
    return 0;
}

//!Duration in millis
quint64 StreamingChunkCacheKey::duration() const
{
    //TODO/IMPL
    return 0;
}

//!startTimestamp() + duration
quint64 StreamingChunkCacheKey::endTimestamp() const
{
    //TODO/IMPL
    return 0;
}

//!Video resolution
QSize StreamingChunkCacheKey::pictureSizePixels() const
{
    //TODO/IMPL
    return QSize();
}

//media format (codec format, container format)
QString StreamingChunkCacheKey::containerFormat() const
{
    //TODO/IMPL
    return QString();
}

QString StreamingChunkCacheKey::videoCodec() const
{
    //TODO/IMPL
    return QString();
}

QString StreamingChunkCacheKey::audioCodec() const
{
    //TODO/IMPL
    return QString();
}

bool StreamingChunkCacheKey::operator<( const StreamingChunkCacheKey& right ) const
{
    //TODO/IMPL
    return false;
}

bool StreamingChunkCacheKey::operator==( const StreamingChunkCacheKey& right ) const
{
    //TODO/IMPL
    return false;
}

uint qHash( const StreamingChunkCacheKey& key )
{
    //TODO/IMPL
    return 0;
}
