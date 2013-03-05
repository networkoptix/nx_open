////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk.h"

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

    //TODO/IMPL should find some common place for containerFormat -> MIME_type match
}

//!Returns whole chunk data
QByteArray StreamingChunk::data() const
{
    QMutexLocker lk( &m_mutex );
    return m_data;
}

bool StreamingChunk::tryRead( SequentialReadingContext* const ctx, QByteArray* const dataBuffer )
{
    QMutexLocker lk( &m_mutex );
    if( ctx->m_currentOffset >= m_data.size() )
        return !m_isOpenedForModification;  //if opened, expecting more data to arrive to chunk
    dataBuffer->append( m_data.mid( ctx->m_currentOffset ) );
    ctx->m_currentOffset = m_data.size();
    return true;
}

//!Only one thread is allowed to modify chunk data at a time
bool StreamingChunk::openForModification()
{
    QMutexLocker lk( &m_mutex );
    if( m_isOpenedForModification )
        return false;
    m_isOpenedForModification = true;
    return true;
}

void StreamingChunk::appendData( const QByteArray& data )
{
    {
        QMutexLocker lk( &m_mutex );
        Q_ASSERT( m_isOpenedForModification );
        m_data.append( data );
    }

    emit newDataIsAvailable( this, data.size() );
}

void StreamingChunk::doneModification( StreamingChunk::ResultCode /*result*/ )
{
    {
        QMutexLocker lk( &m_mutex );
        Q_ASSERT( m_isOpenedForModification );
        m_isOpenedForModification = false;
    }

    emit newDataIsAvailable( this, 0 );
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
