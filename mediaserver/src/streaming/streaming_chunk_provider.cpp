////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_provider.h"

#include <memory>

#include "streaming_chunk_transcoder.h"


static const unsigned int MICROS_PER_SECOND = 1000*1000;

StreamingChunk* StreamingChunkProvider::get( const StreamingChunkCacheKey& key, int* const itemCost )
{
    std::auto_ptr<StreamingChunk> chunk( new StreamingChunk( key ) );
    if( !StreamingChunkTranscoder::instance()->transcodeAsync( key, chunk.get() ) )
        return NULL;

    //NOTE at this time chunk size in bytes is unknown, since transcoding is about to be started
    *itemCost = key.duration() / MICROS_PER_SECOND;
    return chunk.release();
}
