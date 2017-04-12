////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNKPROVIDER_H
#define STREAMINGCHUNKPROVIDER_H

#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"
#include <common/common_module_aware.h>


/*!
    \note Not thread-safe
*/
class StreamingChunkProvider: public QnCommonModuleAware
{
public:
    StreamingChunkProvider(QnCommonModule* commonModule);
    /*!
        Posts a task to transcoder to create required chunk.
        Does not wait for transcoding to complete, so returned chunk is still filled with data, or transcoding could be not even started if chunk time is in future
        \return Returned chunk is in \a opened state. Ownership is passed to the caller
    */
    bool get( const StreamingChunkCacheKey& key, int* const itemCost, StreamingChunkPtr* const chunk );
};

#endif  //STREAMINGCHUNKPROVIDER_H
