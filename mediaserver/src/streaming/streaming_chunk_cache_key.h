////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_CHUNK_CACHE_KEY_H
#define STREAMING_CHUNK_CACHE_KEY_H

#include <map>

#include <QDateTime>
#include <QSize>
#include <QString>


/*!
    Identifies chunk in cache by following parameters:
    - id of resource
    - start/end position
    - media stream format
      - container format
      - video codec type
      - video resolution
*/
class StreamingChunkCacheKey
{
public:
    StreamingChunkCacheKey();
    /*!
        \param channel channel number or -1 for all channels
        \param containerFormat E.g., ts for mpeg2/ts
        \param startTimestamp In micros
        \param endTimestamp In micros
        \param auxiliaryParams
    */
    StreamingChunkCacheKey(
        const QString& uniqueResourceID,
        int channel,
        const QString& containerFormat,
        quint64 startTimestamp,
        quint64 duration,
        const std::multimap<QString, QString>& auxiliaryParams );

    //!data source (camera id, stream id)
    QString srcResourceUniqueID() const;
    unsigned int channel() const;
    //!Chunk start timestamp (micros). This is internal timestamp, not calendar time
    quint64 startTimestamp() const;
    //!Duration in micros
    quint64 duration() const;
    //!startTimestamp() + duration
    quint64 endTimestamp() const;
    //!Video resolution
    QSize pictureSizePixels() const;
    //media format (codec format, container format)
    QString containerFormat() const;
    QString videoCodec() const;
    QString audioCodec() const;

    bool operator<( const StreamingChunkCacheKey& right ) const;
    bool operator==( const StreamingChunkCacheKey& right ) const;
};

uint qHash( const StreamingChunkCacheKey& key );

#endif  //STREAMING_CHUNK_CACHE_KEY_H
