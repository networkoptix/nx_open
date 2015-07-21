////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_cache.h"

#include "media_server/settings.h"


//TODO/HLS: #ak cache cost MUST be measured in bytes, not seconds!

Q_GLOBAL_STATIC( StreamingChunkCache, streamingChunkCacheInstance );

StreamingChunkCache::StreamingChunkCache()
:
    ItemCache<StreamingChunkCacheKey, StreamingChunkPtr, StreamingChunkProvider>(
        MSSettings::roSettings()->value( nx_ms_conf::HLS_CHUNK_CACHE_SIZE_SEC, nx_ms_conf::DEFAULT_MAX_CACHE_COST_SEC).toUInt(),
        new StreamingChunkProvider() ),
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
