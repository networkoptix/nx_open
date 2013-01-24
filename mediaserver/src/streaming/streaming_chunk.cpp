////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk.h"


StreamingChunk::SequentialReadingContext::SequentialReadingContext()
{
    Q_ASSERT( false );
}


StreamingChunk::StreamingChunk( const StreamingChunkCacheKey& params )
:
    m_params( params )
{
    Q_ASSERT( false );
}

const StreamingChunkCacheKey& StreamingChunk::params() const
{
    return m_params;
}

QString StreamingChunk::mimeType() const
{
    //TODO/IMPL
    return QString();
}

//!Returns whole chunk data
QByteArray StreamingChunk::data() const
{
    //TODO/IMPL
    return QByteArray();
}

bool StreamingChunk::tryRead( SequentialReadingContext* const ctx, QByteArray* const dataBuffer )
{
    //TODO/IMPL
    return false;
}

//!Only one thread is allowed to modify chunk data at a time
void StreamingChunk::openForModification()
{
    //TODO/IMPL
}

void StreamingChunk::appendData( const QByteArray& data )
{
    //TODO/IMPL
}

void StreamingChunk::doneModification( StreamingChunk::ResultCode result )
{
    //TODO/IMPL
}
