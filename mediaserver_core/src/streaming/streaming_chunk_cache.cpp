////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_cache.h"
#include "media_server/settings.h"
#include <media_server/media_server_module.h>

//TODO/HLS: #ak cache cost MUST be measured in bytes, not seconds!

StreamingChunkCache::StreamingChunkCache(QnCommonModule* commonModule)
:
    ItemCache<StreamingChunkCacheKey, StreamingChunkPtr, StreamingChunkProvider>(
        qnServerModule->roSettings()->value( nx_ms_conf::HLS_CHUNK_CACHE_SIZE_SEC, nx_ms_conf::DEFAULT_MAX_CACHE_COST_SEC).toUInt(),
        std::unique_ptr<StreamingChunkProvider>(new StreamingChunkProvider(commonModule)) )
{
}
