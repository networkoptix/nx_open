////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_CHUNK_CACHE_H
#define STREAMING_CHUNK_CACHE_H

#include "item_cache.h"
#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"
#include "streaming_chunk_provider.h"


/*!
    Cache cost is measured in seconds
*/
class StreamingChunkCache
:
    public ItemCache<StreamingChunkCacheKey, StreamingChunkPtr, StreamingChunkProvider>
{
public:
    StreamingChunkCache();
    ~StreamingChunkCache();

    static StreamingChunkCache* instance();

private:
    StreamingChunkProvider* m_itemProvider;
};

#endif  //STREAMING_CHUNK_CACHE_H
