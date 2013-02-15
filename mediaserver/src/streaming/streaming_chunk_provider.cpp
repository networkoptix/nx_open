////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_provider.h"

#include <memory>

#include "streaming_chunk_transcoder.h"


StreamingChunk* StreamingChunkProvider::get( const StreamingChunkCacheKey& key, int* const itemCost )
{
    std::auto_ptr<StreamingChunk> chunk( new StreamingChunk( key ) );
    if( !StreamingChunkTranscoder::instance()->transcodeAsync( key, chunk.get() ) )
        return NULL;

    *itemCost = 1;  //TODO/IMPL
    return chunk.release();
}
