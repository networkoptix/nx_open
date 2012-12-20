////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_CHUNK_CACHE_KEY_H
#define STREAMING_CHUNK_CACHE_KEY_H

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
    //!data source (camera id, stream id)
    QString srcResourceUniqueID() const;
    unsigned int channelNumber() const;
    //start date
    /*!
        \return millis since 1970/1/1 00:00, UTC
    */
    quint64 startTimestamp() const;
    //!Duration in millis
    quint64 duration() const;
    //!Video resolution
    QSize pictureSizePixels() const;
    //media format (codec format, container format)
    QString containerFormat() const;
    QString videoCodec() const;
    QString audioCodec() const;

    bool operator<( const StreamingChunkCacheKey& right );
};

#endif  //STREAMING_CHUNK_CACHE_KEY_H
