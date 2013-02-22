////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_cache.h"


static const unsigned int MAX_CACHE_COST = 1800;    //TODO move to some setting

Q_GLOBAL_STATIC( StreamingChunkCache, streamingChunkCacheInstance );

//TODO/IMPL/HLS cache cost MUST be measured in bytes, not seconds!

StreamingChunkCache::StreamingChunkCache()
:
    ItemCache<StreamingChunkCacheKey, StreamingChunk, StreamingChunkProvider>( MAX_CACHE_COST, new StreamingChunkProvider() ),
    m_itemProvider( itemProvider() )
{
}

StreamingChunkCache::~StreamingChunkCache()
{
    delete m_itemProvider;
    m_itemProvider = NULL;
}

StreamingChunkCache* StreamingChunkCache::instance()
{
    return streamingChunkCacheInstance();
}
