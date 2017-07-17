////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNKPROVIDER_H
#define STREAMINGCHUNKPROVIDER_H

#include <nx/utils/move_only_func.h>

#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"

class StreamingChunkTranscoder;

class AbstractStreamingChunkProvider
{
public:
    virtual ~AbstractStreamingChunkProvider() = default;

    /**
        Posts a task to transcoder to create required chunk.
        Does not wait for transcoding to complete, so returned chunk is still being filled with data, 
        or transcoding could be not even started if chunk time is in future.
        @return Returned chunk is in opened state. Ownership is passed to the caller.
    */
    virtual bool get(
        const StreamingChunkCacheKey& key,
        int* const itemCost,
        StreamingChunkPtr* const chunk) = 0;
};

//-------------------------------------------------------------------------------------------------

/*!
    \note Not thread-safe
*/
class StreamingChunkProvider:
    public AbstractStreamingChunkProvider
{
public:
    StreamingChunkProvider();

    virtual bool get(
        const StreamingChunkCacheKey& key,
        int* const itemCost,
        StreamingChunkPtr* const chunk) override;

private:
    StreamingChunkTranscoder* m_transcoder;
};

//-------------------------------------------------------------------------------------------------

class StreamingChunkProviderFactory
{
public:
    using Function = 
        nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractStreamingChunkProvider>()>;

    std::unique_ptr<AbstractStreamingChunkProvider> create();

    Function setCustomFunc(Function);

    static StreamingChunkProviderFactory& instance();

private:
    Function m_customFunc;
};

#endif  //STREAMINGCHUNKPROVIDER_H
