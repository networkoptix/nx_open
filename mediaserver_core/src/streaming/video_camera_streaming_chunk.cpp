#include "video_camera_streaming_chunk.h"

#include <api/helpers/camera_id_helper.h>
#include <core/resource/camera_resource.h>

VideoCameraStreamingChunk::VideoCameraStreamingChunk(
    QnResourcePool* resourcePool,
    const StreamingChunkCacheKey& params,
    std::size_t maxInternalBufferSize)
    :
    StreamingChunk(params, maxInternalBufferSize),
    m_resourcePool(resourcePool)
{
    const auto res = nx::camera_id_helper::findCameraByFlexibleId(
        m_resourcePool,
        params.srcResourceUniqueID());
    if (res)
        m_videoCameraLocker = qnCameraPool->getVideoCameraLockerByResourceId(res->getId());
}

void VideoCameraStreamingChunk::doneModification(ResultCode result)
{
    base_type::doneModification(result);

    m_videoCameraLocker.reset();
}
