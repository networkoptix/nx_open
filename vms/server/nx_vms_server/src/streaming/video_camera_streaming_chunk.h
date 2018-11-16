#pragma once

#include <core/resource_management/resource_pool.h>

#include "streaming_chunk.h"
#include "camera/camera_pool.h"
#include <media_server/media_server_module.h>

/**
 * Locks corresponding VideoCamera object while chunk is still being modified.
 */
class VideoCameraStreamingChunk:
    public StreamingChunk
{
    using base_type = StreamingChunk;

public:
    VideoCameraStreamingChunk(
        QnMediaServerModule* serverModule,
        const StreamingChunkCacheKey& params,
        std::size_t maxInternalBufferSize);

    virtual void doneModification(ResultCode result) override;

private:
    std::unique_ptr<VideoCameraLocker> m_videoCameraLocker;
};
