#pragma once

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
        const StreamingChunkCacheKey& params,
        std::size_t maxInternalBufferSize);

    virtual void doneModification(ResultCode result) override;

private:
    std::unique_ptr<VideoCameraLocker> m_videoCameraLocker;
};
