////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_cache.h"


static const unsigned int MAX_CACHE_COST = 3600;    //TODO move to some setting

Q_GLOBAL_STATIC( StreamingChunkCache, streamingChunkCacheInstance );

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
