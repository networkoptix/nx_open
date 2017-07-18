#pragma once

#include <core/resource_management/resource_pool.h>

#include "streaming_chunk.h"
#include "camera/camera_pool.h"

/**
 * Locks corresponding VideoCamera object while chunk is still being modified.
 */
class VideoCameraStreamingChunk:
    public StreamingChunk
{
    using base_type = StreamingChunk;

public:
    VideoCameraStreamingChunk(
        QnResourcePool* resourcePool,
        const StreamingChunkCacheKey& params,
        std::size_t maxInternalBufferSize);

    virtual void doneModification(ResultCode result) override;

private:
    QnResourcePool* m_resourcePool;
    std::unique_ptr<VideoCameraLocker> m_videoCameraLocker;
};
