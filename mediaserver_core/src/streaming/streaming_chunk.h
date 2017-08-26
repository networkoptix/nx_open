#pragma once

#include <fstream>
#include <memory>
#include <set>

#include <QByteArray>
#include <QString>

#include <nx/network/buffer.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/byte_stream/pipeline.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include "streaming_chunk_cache_key.h"

//#define DUMP_CHUNK_TO_FILE

class StreamingChunk;
using StreamingChunkPtr = std::shared_ptr<StreamingChunk>;

/**
* Chunk of media data, ready to be used by some streaming protocol (e.g., hls).
* In general, it is transcoded small (~10s) part of archive chunk.

* Chunk can actually not contain data, but only expect some data to arrive.
* Chunk, which is still being filled by data is "opened". Chunk already filled is "closed".

* WARNING: Object of this class MUST be used as std::shared_ptr.
* WARNING: It is required to connect to this class's signals using Qt::DirectConnection only.
* NOTE: Class methods are thread-safe.
*/
class StreamingChunk:
    public std::enable_shared_from_this<StreamingChunk>
{
public:
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

    StreamingChunk(
        const StreamingChunkCacheKey& params,
        std::size_t maxInternalBufferSize);
    virtual ~StreamingChunk() = default;

    const StreamingChunkCacheKey& params() const;

    QString mimeType() const;

    /**
    * @return whole chunk data.
    */
    nx::Buffer data() const;

    bool startsWithZeroOffset() const;
    /**
    * Sequential reading.
    * Appends data to dataBuffer.
    * End-of stream is signalled with returning no data and returning true.
    * @param ctx Used to save position.
    * @return true, if some data has been read or end-of file has been reached.
    *   false, if no data in chunk, but some data may arrive in future.
    * NOTE: If chunk is not being modified, then first call returns whole chunk data
    * TODO: Use container that do not require copying to get array substring.
    */
    bool tryRead(
        SequentialReadingContext* const ctx,
        nx::Buffer* const dataBuffer,
        std::size_t maxBytesToRead);
    /** Blocks until data for read context ctx is available. */
    void waitUntilDataAfterOffsetAvailable(const SequentialReadingContext& ctx);

    /**
    * Only one thread is allowed to modify chunk data at a time.
    * @return false, if chunk already opened for modification.
    */
    bool openForModification();
    /** Returns false if internal buffer is filled. Will return true when some data has been read from chunk. */
    bool wantMoreData() const;
    void appendData(const nx::Buffer& data);
    virtual void doneModification(ResultCode result);
    bool isClosed() const;
    size_t sizeInBytes() const;
    /**
    * Blocks until chunk is non-empty and closed or internal buffer has been filled.
    * NOTE: (StreamingChunk::isClosed returns true and StreamingChunk::sizeInBytes() > 0).
    * NOTE: If it already closed, then returns immediately.
    * @return true if chunk is fully generated. false otherwise
    */
    bool waitForChunkReadyOrInternalBufferFilled();
    /** Disables check on internal buffer size. */
    void disableInternalBufferLimit();

private:
    enum class State
    {
        /** Chunk has not been opened for modification yet. */
        init,
        /** Chunk is being filled by data (e.g., from transcoder). */
        opened,
        /** Chunk contains ready-to-download data. */
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
    /**
    * Data offset corresponding to the beginning of m_data.
    * Can be greater then zero if data is removed from the front of the buffer.
    * due to very large chunk size (e.g., in case of using it for export).
    */
    quint64 m_dataOffsetAtTheFrontOfTheBuffer;
    std::set<SequentialReadingContext*> m_readers;
};

class AbstractInputByteStream
{
public:
    virtual ~AbstractInputByteStream() {}

    /**
    * Sequential reading.
    * Appends data to dataBuffer.
    * End-of stream is signalled with returning no data and returning true.
    * @return true, if some data has been read or end-of file has been reached.
    * false, if no data has been read (more data may be available in the future).
    */
    virtual bool tryRead(nx::Buffer* const dataBuffer, std::size_t maxBytesToRead) = 0;
};

/**
* Reads from StreamingChunk.
* NOTE: Not thread-safe!
*/
class StreamingChunkInputStream:
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
