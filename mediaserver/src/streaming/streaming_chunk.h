////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNK_H
#define STREAMINGCHUNK_H

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QString>

#include "streaming_chunk_cache_key.h"


//!Chunk of media data, ready to be used by some streaming protocol (e.g., hls). 
/*!
    In general, it is transcoded small (~10s) part of archive chunk.

    Chunk can actually not contain data, but only expect some data to arrive.
    Chunk, which is still being filled by data is "opened". Chunk already filled is "closed".

    \note Class methods are thread-safe
    \note It is recommended to connect to this class signals using direct connection
*/
class StreamingChunk
:
    public QObject
{
    Q_OBJECT

public:
    //!For sequential chunk reading 
    class SequentialReadingContext
    {
        friend class StreamingChunk;

    public:
        SequentialReadingContext();

    private:
        int m_currentOffset;
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
    virtual ~StreamingChunk();

    const StreamingChunkCacheKey& params() const;

    QString mimeType() const;

    //!Returns whole chunk data
    QByteArray data() const;

    //!Sequential reading
    /*!
        Appends data to \a dataBuffer.
        End-of stream is signalled with returning no data and returning \a true
        \param ctx Used to save position
        \return true, if some data has been read or end-of file has been reached. false, if no data in chunk, but some data may arrive in future
        \note If chunk is not being modified, then first call returns whole chunk data
        \todo Use container that do not require copying to get array substring
    */
    bool tryRead( SequentialReadingContext* const ctx, QByteArray* const dataBuffer );

    //!Only one thread is allowed to modify chunk data at a time
    /*!
        \return false, if chunk already opened for modification
    */
    bool openForModification();
    //!Add more data to chunk
    /*!
        Emits signal \a newDataIsAvailable
    */
    void appendData( const QByteArray& data );
    //!
    void doneModification( ResultCode result );
    bool isClosed() const;
    size_t sizeInBytes() const;

signals:
    /*!
        \param pThis \a this for a case of direct connection
        \param newSizeBytes new size of chunk (in bytes)
    */
    void newDataIsAvailable( StreamingChunk* pThis, quint64 newSizeBytes );

private:
    StreamingChunkCacheKey m_params;
    mutable QMutex m_mutex;
    QByteArray m_data;
    bool m_isOpenedForModification;
};

#endif  //STREAMINGCHUNK_H
