////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNK_H
#define STREAMINGCHUNK_H

#include <fstream>
#include <memory>
#include <set>

#include <QByteArray>
#include <QObject>
#include <QSize>
#include <QString>

#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>
#include <utils/network/buffer.h>
#include <utils/network/http/httptypes.h>

#include "streaming_chunk_cache_key.h"

//#define DUMP_CHUNK_TO_FILE


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
        SequentialReadingContext(StreamingChunk*);
        ~SequentialReadingContext();

    private:
        quint64 m_currentOffset;
        StreamingChunk* m_chunk;

        SequentialReadingContext(const SequentialReadingContext&);
        SequentialReadingContext& operator=(const SequentialReadingContext&);
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
    nx::Buffer data() const;

    //!Sequential reading
    /*!
        Appends data to \a dataBuffer.
        End-of stream is signalled with returning no data and returning \a true
        \param ctx Used to save position
        \return true, if some data has been read or end-of file has been reached. false, if no data in chunk, but some data may arrive in future
        \note If chunk is not being modified, then first call returns whole chunk data
        \todo Use container that do not require copying to get array substring
    */
    bool tryRead(
        SequentialReadingContext* const ctx,
        nx::Buffer* const dataBuffer,
        std::size_t maxBytesToRead);
    /** Blocks until data for read context \a ctx is available */
    void waitUntilDataAfterOffsetAvailable(const SequentialReadingContext& ctx);

    //!Only one thread is allowed to modify chunk data at a time
    /*!
        \return false, if chunk already opened for modification
    */
    bool openForModification();
    //!Returns \a false if internal buffer is filled. Will return \a true when some data has been read from chunk
    bool wantMoreData() const;
    //!Add more data to chunk
    void appendData( const nx::Buffer& data );
    //!
    void doneModification( ResultCode result );
    bool isClosed() const;
    size_t sizeInBytes() const;
    //!Blocks until chunk is non-empty and closed or internal buffer has been filled
    /*!
        \note (\a StreamingChunk::isClosed returns \a true and \a StreamingChunk::sizeInBytes() > 0)
        \note If it already closed, then returns immediately
        \return \a true if chunk is fully generated. \a false otherwise
    */
    bool waitForChunkReadyOrInternalBufferFilled();
    /** Disables check on internal buffer size */
    void disableInternalBufferLimit();

private:
    enum class State
    {
        //!Chunk has not been opened for modification yet
        init,
        //!chunk is being filled by data (e.g., from transcoder)
        opened,
        //!chunk contains ready-to-download data
        closed
    };

    StreamingChunkCacheKey m_params;
    mutable QnMutex m_mutex;
    nx::Buffer m_data;
    State m_modificationState;
    QnWaitCondition m_cond;
#ifdef DUMP_CHUNK_TO_FILE
    std::ofstream m_dumpFile;
#endif
    std::size_t m_maxInternalBufferSize;
    /** Data offset corresponding to the beginning of \a m_data.
        Can be greater then zero if data is removed from the front of the buffer 
        due to very large chunk size (e.g., in case of using it for export)
    */
    quint64 m_dataOffsetAtTheFrontOfTheBuffer;
    std::set<SequentialReadingContext*> m_readers;
};


class AbstractInputByteStream
{
public:
    virtual ~AbstractInputByteStream() {}

    //!Sequential reading
    /*!
        Appends data to \a dataBuffer.
        End-of stream is signalled with returning no data and returning \a true
        \return true, if some data has been read or end-of file has been reached. false, if no data has been read (more data may be available in the future)
    */
    virtual bool tryRead(nx::Buffer* const dataBuffer, std::size_t maxBytesToRead) = 0;
};

//!Reads from \a StreamingChunk
/*!
   \note Not thread-safe! 
*/
class StreamingChunkInputStream
:
    public AbstractInputByteStream
{
public:
    StreamingChunkInputStream(StreamingChunk* chunk);

    virtual bool tryRead(nx::Buffer* const dataBuffer, std::size_t maxBytesToRead) override;

    void setByteRange(const nx_http::header::ContentRange& range);
    void waitForSomeDataAvailable();

private:
    StreamingChunk* m_chunk;
    StreamingChunk::SequentialReadingContext m_readCtx;
    boost::optional<nx_http::header::ContentRange> m_range;
};

#endif  //STREAMINGCHUNK_H
