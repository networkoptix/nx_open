#pragma once

#include <nx/utils/move_only_func.h>

#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"

class QnResourcePool;
class StreamingChunkTranscoder;

class AbstractStreamingChunkProvider
{
public:
    virtual ~AbstractStreamingChunkProvider() = default;

    /**
     * Posts a task to transcoder to create required chunk.
     * Does not wait for transcoding to complete, so returned chunk is still being filled with data,
     * or transcoding could be not even started if chunk time is in future.
     * @return Returned chunk is in opened state. Ownership is passed to the caller.
     */
    virtual bool get(
        const StreamingChunkCacheKey& key,
        int* const itemCost,
        StreamingChunkPtr* const chunk) = 0;
};

//-------------------------------------------------------------------------------------------------

class StreamingChunkProvider:
    public AbstractStreamingChunkProvider
{
public:
    StreamingChunkProvider(
        QnResourcePool* resourcePool,
        StreamingChunkTranscoder* transcoder);

    virtual bool get(
        const StreamingChunkCacheKey& key,
        int* const itemCost,
        StreamingChunkPtr* const chunk) override;

private:
    QnResourcePool* m_resourcePool;
    StreamingChunkTranscoder* m_transcoder;
};

//-------------------------------------------------------------------------------------------------

class StreamingChunkProviderFactory
{
public:
    using Function =
        nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractStreamingChunkProvider>()>;

    std::unique_ptr<AbstractStreamingChunkProvider> create(
        QnResourcePool* resourcePool,
        StreamingChunkTranscoder* transcoder);

    Function setCustomFunc(Function);

    static StreamingChunkProviderFactory& instance();

private:
    Function m_customFunc;
};
