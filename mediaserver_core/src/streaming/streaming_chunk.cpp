////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk.h"

#include <atomic>
#include <limits>

#include <nx/utils/random.h>

#include <api/helpers/camera_id_helper.h>
#include <core/resource/security_cam_resource.h>
#include <core/resource_management/resource_pool.h>
#include "media_server/settings.h"
#include <media_server/media_server_module.h>

StreamingChunk::SequentialReadingContext::SequentialReadingContext(StreamingChunk* chunk):
    m_currentOffset( 0 ),
    m_chunk(chunk)
{
    m_chunk->m_readers.insert(this);
}

StreamingChunk::SequentialReadingContext::~SequentialReadingContext()
{
    m_chunk->m_readers.erase(this);
}

StreamingChunk::StreamingChunk(
    QnResourcePool* resPool,
    const StreamingChunkCacheKey& params):
    m_params(params),
    m_modificationState( State::init ),
    m_maxInternalBufferSize(
        qnServerModule->roSettings()->value(
            nx_ms_conf::HLS_MAX_CHUNK_BUFFER_SIZE,
            nx_ms_conf::DEFAULT_HLS_MAX_CHUNK_BUFFER_SIZE).toUInt() ),
    m_dataOffsetAtTheFrontOfTheBuffer(0)
{
    const auto res = nx::camera_id_helper::findCameraByFlexibleId(resPool, params.srcResourceUniqueID());
    if (res)
        m_videoCameraLocker = qnCameraPool->getVideoCameraLockerByResourceId(res->getId());
}

StreamingChunk::~StreamingChunk()
{
}

const StreamingChunkCacheKey& StreamingChunk::params() const
{
    return m_params;
}

QString StreamingChunk::mimeType() const
{
    if( m_params.containerFormat() == "mpegts" )
        return QLatin1String("video/mp2t");
    else if( m_params.containerFormat() == "mp4" )
        return QLatin1String("video/mp4");
    else if( m_params.containerFormat() == "webm" )
        return QLatin1String("video/webm");
    else if( m_params.containerFormat() == "flv" )
        return QLatin1String("video/x-flv");
    else
        return QLatin1String("application/octet-stream");

    //TODO #ak should find some common place for containerFormat -> MIME_type match
}

//!Returns whole chunk data
nx::Buffer StreamingChunk::data() const
{
    QnMutexLocker lk( &m_mutex );
    return m_data;
}

bool StreamingChunk::tryRead(
    SequentialReadingContext* const ctx,
    nx::Buffer* const dataBuffer,
    std::size_t maxBytesToRead)
{
    QnMutexLocker lk(&m_mutex);

    const quint64 dataOffsetAtTheEndOfTheBuffer =
        m_dataOffsetAtTheFrontOfTheBuffer + m_data.size();

    if (ctx->m_currentOffset >= dataOffsetAtTheEndOfTheBuffer)  //all data has been read
        return m_modificationState != State::opened;    //if chunk not opened, signalling end-of-data.
                                                        //Else, expecting more data to arrive to chunk

    Q_ASSERT(ctx->m_currentOffset >= m_dataOffsetAtTheFrontOfTheBuffer);
    const quint64 bytesToCopy = std::min<quint64>(
        maxBytesToRead,
        dataOffsetAtTheEndOfTheBuffer - ctx->m_currentOffset);
    dataBuffer->append(
        m_data.mid(
            ctx->m_currentOffset - m_dataOffsetAtTheFrontOfTheBuffer,
            (int)bytesToCopy));
    ctx->m_currentOffset += bytesToCopy;

    if (m_data.size() < m_maxInternalBufferSize)
        return true;

    //cleaning up space in m_data
    //  checking that there is no read context that reads data we want to clean
    const auto readerWithMinOffsetIter = std::min_element(
        m_readers.begin(),
        m_readers.end(),
        [](const SequentialReadingContext* left, const SequentialReadingContext* right)
        {
            return left->m_currentOffset < right->m_currentOffset;
        });
    Q_ASSERT(readerWithMinOffsetIter != m_readers.end());
    Q_ASSERT((*readerWithMinOffsetIter)->m_currentOffset >= m_dataOffsetAtTheFrontOfTheBuffer);

    const quint64 dataInUseSize =
        dataOffsetAtTheEndOfTheBuffer - (*readerWithMinOffsetIter)->m_currentOffset;
    Q_ASSERT(dataInUseSize <= (quint64)m_data.size());
    const auto unusedBytesInFrontOfTheBuffer = (quint64)m_data.size() - dataInUseSize;
    m_data.remove(0, unusedBytesInFrontOfTheBuffer);
    m_dataOffsetAtTheFrontOfTheBuffer += unusedBytesInFrontOfTheBuffer;

    return true;
}

void StreamingChunk::waitUntilDataAfterOffsetAvailable(
    const SequentialReadingContext& ctx)
{
    QnMutexLocker lk(&m_mutex);
    for (;;)
    {
        if (m_dataOffsetAtTheFrontOfTheBuffer + m_data.size() > ctx.m_currentOffset)
            return;
        if (m_modificationState != State::opened)
            return; //chunk is done, no data will arrive
        m_cond.wait(lk.mutex());
    }
}

#ifdef DUMP_CHUNK_TO_FILE
static std::atomic<int> fileNumber = 1;
static QString filePathBase( lit("c:\\tmp\\chunks\\%1_%2.ts").arg(nx::utils::random::number()) );
#endif

//!Only one thread is allowed to modify chunk data at a time
bool StreamingChunk::openForModification()
{
    QnMutexLocker lk( &m_mutex );
    if( m_modificationState == State::opened )
        return false;
    m_modificationState = State::opened;

#ifdef DUMP_CHUNK_TO_FILE
    m_dumpFile.open(
        filePathBase.arg( fileNumber++, 2, 10, QChar(L'0') ).toStdString(),
        std::ios_base::binary | std::ios_base::out );
#endif

    return true;
}

bool StreamingChunk::wantMoreData() const
{
    QnMutexLocker lk(&m_mutex);
    return (std::size_t)m_data.size() < m_maxInternalBufferSize;
}

void StreamingChunk::appendData( const nx::Buffer& data )
{
    static const size_t BUF_INCREASE_STEP = 128*1024;

    {
        QnMutexLocker lk( &m_mutex );
        NX_ASSERT( m_modificationState == State::opened );
        if( m_data.capacity() < m_data.size() + data.size() )
            m_data.reserve( m_data.size() + data.size() + BUF_INCREASE_STEP );
        m_data.append( data );
        m_cond.wakeAll();
    }

#ifdef DUMP_CHUNK_TO_FILE
    m_dumpFile.write( data.constData(), data.size() );
#endif
}

void StreamingChunk::doneModification( StreamingChunk::ResultCode /*result*/ )
{
    {
        QnMutexLocker lk( &m_mutex );
        NX_ASSERT( m_modificationState == State::opened );
        m_modificationState = State::closed;
        m_cond.wakeAll();
    }

    m_videoCameraLocker.reset();

#ifdef DUMP_CHUNK_TO_FILE
    m_dumpFile.close();
#endif
}

bool StreamingChunk::isClosed() const
{
    QnMutexLocker lk( &m_mutex );
    return m_modificationState != State::opened;
}

size_t StreamingChunk::sizeInBytes() const
{
    QnMutexLocker lk( &m_mutex );
    return m_data.size();
}

bool StreamingChunk::waitForChunkReadyOrInternalBufferFilled()
{
    QnMutexLocker lk( &m_mutex );
    //waiting while chunk is modified
    for (;;)
    {
        if (m_modificationState >= State::closed)
            return true;    //whole chunk has been generated
        if ((std::size_t)m_data.size() >= m_maxInternalBufferSize)
            return false;   //no space in internal buffer
        m_cond.wait(lk.mutex());
    }
}

void StreamingChunk::disableInternalBufferLimit()
{
    QnMutexLocker lk(&m_mutex);
    m_maxInternalBufferSize =
        std::numeric_limits<decltype(m_maxInternalBufferSize)>::max();
}

//////////////////////////////////////////////
//   StreamingChunkInputStream
//////////////////////////////////////////////

StreamingChunkInputStream::StreamingChunkInputStream(StreamingChunk* chunk):
    m_chunk(chunk),
    m_readCtx(chunk)
{
}

bool StreamingChunkInputStream::tryRead(
    nx::Buffer* const dataBuffer,
    std::size_t maxBytesToRead)
{
    if ((!m_range) ||     //no range specified
        ((m_range.get().rangeSpec.start == 0)
            && (m_range.get().rangeLength() == m_chunk->sizeInBytes()))) //full entity requested
    {
        return m_chunk->tryRead(&m_readCtx, dataBuffer, maxBytesToRead);
    }

    NX_ASSERT( m_chunk->isClosed() && m_chunk->sizeInBytes() > 0 );
    //supporting byte range only on closed chunk
    if (!(m_chunk->isClosed() && m_chunk->sizeInBytes() > 0))
        return false;

    const nx::Buffer& chunkData = m_chunk->data();
    dataBuffer->append(chunkData.mid(
        m_range.get().rangeSpec.start,
        m_range.get().rangeSpec.end
        ? (m_range.get().rangeSpec.end.get() - m_range.get().rangeSpec.start)
        : -1));

    return true;
}

void StreamingChunkInputStream::setByteRange(const nx_http::header::ContentRange& range)
{
    m_range = range;
}

void StreamingChunkInputStream::waitForSomeDataAvailable()
{
    m_chunk->waitUntilDataAfterOffsetAvailable(m_readCtx);
}
