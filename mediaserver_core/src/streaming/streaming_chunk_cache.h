////////////////////////////////////////////////////////////
// 21 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_CHUNK_CACHE_H
#define STREAMING_CHUNK_CACHE_H

#include "item_cache.h"
#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"
#include "streaming_chunk_provider.h"
#include <common/common_module.h>


/*!
    Cache cost is measured in seconds
*/
class StreamingChunkCache:
    public QObject,
    public ItemCache<StreamingChunkCacheKey, StreamingChunkPtr, StreamingChunkProvider>
{
    Q_OBJECT
public:
    StreamingChunkCache(QnCommonModule* commonModule);
};

#endif  //STREAMING_CHUNK_CACHE_H
