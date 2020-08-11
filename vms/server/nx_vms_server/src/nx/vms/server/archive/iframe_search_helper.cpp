#include "iframe_search_helper.h"

#include <camera/camera_pool.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <camera/video_camera.h>

using namespace nx::vms::api;

namespace nx::vms::server::archive {

IframeSearchHelper::IframeSearchHelper(
    const QnResourcePool* resourcePool,
    const QnVideoCameraPool* cameraPool)
    :
    m_resourcePool(resourcePool),
    m_cameraPool(cameraPool)
{
}

qint64 IframeSearchHelper::findAfter(
    const QnUuid& deviceId,
    nx::vms::api::StreamIndex streamIndex,
    qint64 timestampUs) const
{
    const auto resource =
        m_resourcePool->getResourceById(deviceId).dynamicCast<QnSecurityCamResource>();
    if (!resource)
        return -1;

    const auto camera = m_cameraPool->getVideoCamera(resource);
    if (!camera)
        return -1;

    const auto iFrame = camera->getIFrameByTime(
        streamIndex,
        timestampUs,
        /*channel*/ 0,
        nx::api::ImageRequest::RoundMethod::iFrameAfter);
    if (!iFrame)
        return -1;

    return iFrame->timestamp;
}

} // namespace nx::vms::server::archive
