////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNK_H
#define STREAMINGCHUNK_H

#include <memory>

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QString>

#include "streaming_chunk_cache_key.h"


class StreamingChunk;
typedef std::shared_ptr<StreamingChunk> StreamingChunkPtr;

//!Chunk of media data, ready to be used by some streaming protocol (e.g., hls). 
/*!
    In general, it is transcoded small (~10s) part of archive chunk.

    Chunk can actually not contain data, but only expect some data to arrive.
    Chunk, which is still being filled by data is "opened". Chunk already filled is "closed".

    \warning Object of this class MUST be used as std::shared_ptr
    \warning It is required to connect to this class's signals using Qt::DirectConnection only
    \note Class methods are thread-safe
*/
class StreamingChunk
:
    public QObject,
    public std::enable_shared_from_this<StreamingChunk>
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

    //!Disconnects \a receiver from all signals of this class and waits for issued \a emit calls connected to \a receiver to return
    /*!
        TODO #ak: move to some base class
    */
    void disconnectAndJoin( QObject* receiver );

signals:
    /*!
        \param pThis \a this for a case of direct connection
        \param newSizeBytes new size of chunk (in bytes)
    */
    void newDataIsAvailable( StreamingChunkPtr chunk, quint64 newSizeBytes );

private:
    StreamingChunkCacheKey m_params;
    mutable QMutex m_mutex;
    QByteArray m_data;
    bool m_isOpenedForModification;
    mutable QMutex m_signalEmitMutex;
};

#endif  //STREAMINGCHUNK_H
