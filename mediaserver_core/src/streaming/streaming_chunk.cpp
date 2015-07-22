////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk.h"

#include <atomic>

#include <QMutexLocker>


StreamingChunk::SequentialReadingContext::SequentialReadingContext()
:
    m_currentOffset( 0 )
{
}

StreamingChunk::StreamingChunk( const StreamingChunkCacheKey& params )
:
    m_params( params ),
    m_isOpenedForModification( false )
{
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
    QMutexLocker lk( &m_mutex );
    return m_data;
}

bool StreamingChunk::tryRead( SequentialReadingContext* const ctx, nx::Buffer* const dataBuffer )
{
    QMutexLocker lk( &m_mutex );
    if( ctx->m_currentOffset >= m_data.size() ) //all data has been read
        return !m_isOpenedForModification;      //if opened, expecting more data to arrive to chunk
    dataBuffer->append( m_data.mid( ctx->m_currentOffset ) );
    ctx->m_currentOffset = m_data.size();
    return true;
}

#ifdef DUMP_CHUNK_TO_FILE
static std::atomic<int> fileNumber = 1;
static QString filePathBase( lit("c:\\tmp\\chunks\\%1_%2.ts").arg(rand()) );
#endif

//!Only one thread is allowed to modify chunk data at a time
bool StreamingChunk::openForModification()
{
    QMutexLocker lk( &m_mutex );
    if( m_isOpenedForModification )
        return false;
    m_isOpenedForModification = true;

#ifdef DUMP_CHUNK_TO_FILE
    m_dumpFile.open(
        filePathBase.arg( ++fileNumber ).toStdString(),
        std::ios_base::binary | std::ios_base::out );
#endif

    return true;
}

void StreamingChunk::appendData( const nx::Buffer& data )
{
    static const size_t BUF_INCREASE_STEP = 128*1024;

    {
        QMutexLocker lk( &m_mutex );
        Q_ASSERT( m_isOpenedForModification );
        if( m_data.capacity() < m_data.size() + data.size() )
            m_data.reserve( m_data.size() + data.size() + BUF_INCREASE_STEP );
        m_data.append( data );
    }

#ifdef DUMP_CHUNK_TO_FILE
    m_dumpFile.write( data.constData(), data.size() );
#endif

    QMutexLocker lk( &m_signalEmitMutex );
    emit newDataIsAvailable( shared_from_this(), data.size() );
}

void StreamingChunk::doneModification( StreamingChunk::ResultCode /*result*/ )
{
    {
        QMutexLocker lk( &m_mutex );
        Q_ASSERT( m_isOpenedForModification );
        m_isOpenedForModification = false;
        m_cond.wakeAll();
    }

#ifdef DUMP_CHUNK_TO_FILE
    m_dumpFile.close();
#endif

    QMutexLocker lk( &m_signalEmitMutex );
    emit newDataIsAvailable( shared_from_this(), 0 );
}

bool StreamingChunk::isClosed() const
{
    QMutexLocker lk( &m_mutex );
    return !m_isOpenedForModification;
}

size_t StreamingChunk::sizeInBytes() const
{
    QMutexLocker lk( &m_mutex );
    return m_data.size();
}

void StreamingChunk::waitForChunkReady()
{
    QMutexLocker lk( &m_mutex );
    while( (m_data.size() == 0) || m_isOpenedForModification )
        m_cond.wait( lk.mutex() );
}

void StreamingChunk::disconnectAndJoin( QObject* receiver )
{
    disconnect( this, nullptr, receiver, nullptr );
    QMutexLocker lk( &m_signalEmitMutex );  //waiting for signals to be emitted in other threads
}


//////////////////////////////////////////////
//   StreamingChunkInputStream
//////////////////////////////////////////////

StreamingChunkInputStream::StreamingChunkInputStream( StreamingChunk* chunk )
:
    m_chunk( chunk )
{
}

bool StreamingChunkInputStream::tryRead( nx::Buffer* const dataBuffer )
{
    if( (!m_range) ||     //no range specified
        ((m_range.get().rangeSpec.start == 0) && (m_range.get().rangeLength() == m_chunk->sizeInBytes())) ) //full entity requested
    {
        return m_chunk->tryRead( &m_readCtx, dataBuffer );
    }

    assert( m_chunk->isClosed() && m_chunk->sizeInBytes() > 0 );
    //supporting byte range only on closed chunk
    if( !(m_chunk->isClosed() && m_chunk->sizeInBytes() > 0) )
        return false;

    const nx::Buffer& chunkData = m_chunk->data();
    dataBuffer->append( chunkData.mid(
        m_range.get().rangeSpec.start,
        m_range.get().rangeSpec.end ? (m_range.get().rangeSpec.end.get() - m_range.get().rangeSpec.start) : -1 ) );

    return true;
}

void StreamingChunkInputStream::setByteRange( const nx_http::header::ContentRange& range )
{
    m_range = range;
}
