////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNK_H
#define STREAMINGCHUNK_H

#include <QObject>
#include <QSize>
#include <QString>

#include "streaming_chunk_cache_key.h"


/*!
    Chunk of data, ready to be used by some streaming protocol (e.g., hls). 
    In general, it is transcoded small (~10s) part of archive chunk.

    Chunk can actually not contain data, but only expect some data to arrive.
    Chunk, which is still being filled by data is "opened". Chunk already filled is "closed".

    \note Class methods are thread-safe
*/
class StreamingChunk
:
    public QObject
{
    Q_OBJECT

public:
    class SequentialReadingContext
    {
    public:
        SequentialReadingContext();
    };

    enum State
    {
        //!chunk is being filled by data (e.g., from transcoder)
        opened,
        //!chunk contains ready-to-download data
        closed
    };

    enum ResultCode
    {
        rcEndOfData,
        rcError
    };

    StreamingChunk( const StreamingChunkCacheKey& params );

    const StreamingChunkCacheKey& params() const;

    ////!Data source (camera id, stream id)
    //QString srcResourceUniqueID() const;
    //unsigned int channelNumber() const;
    ////!Start date
    ///*!
    //    \return millis since 1970/1/1 00:00, UTC
    //*/
    //quint64 startTimestamp() const;
    ////!Duration in millis
    //quint64 duration() const;
    ////!Video resolution
    //QSize pictureSizePixels() const;
    ////!Media format (codec format, container format)
    //QString containerFormat() const;
    //QString videoCodec() const;
    //QString audioCodec() const;
    QString mimeType() const;

    //!Returns whole chunk data
    QByteArray data() const;

    //!sequential reading
    /*!
        Appends data to \a dataBuffer.
        End-of stream is signalled with returning no data and returning \a true
        \param ctx Used to save position
        \return true, if data read. false, if no data in chunk
        \note If chunk is not being modified, then first call returns whole chunk data
        \todo Use container that do not require copying to get array substring
    */
    bool tryRead( SequentialReadingContext* const ctx, QByteArray* const dataBuffer );

    //!Only one thread is allowed to modify chunk data at a time
    void openForModification();
    void appendData( const QByteArray& data );
    void doneModification( ResultCode result );

signals:
    /*!
        \param pThis \a this for a case of direct connection
        \param newSizeBytes new size of chunk (in bytes)
    */
    void newDataIsAvailable( StreamingChunk* pThis, quint64 newSizeBytes );

private:
    StreamingChunkCacheKey m_params;
};

#endif  //STREAMINGCHUNK_H
