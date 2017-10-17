#include "proxy_analytics_driver.h"

#include <nx/client/desktop/analytics/camera_metadata_analytics_controller.h>

namespace nx {
namespace client {
namespace desktop {

ProxyAnalyticsDriver::ProxyAnalyticsDriver(const QnVirtualCameraResourcePtr& camera,
    QObject* parent)
    :
    base_type(parent),
    m_camera(camera)
{
    connect(qnMetadataAnalyticsController,
        &MetadataAnalyticsController::rectangleAddedOrChanged,
        this,
        [this](const QnVirtualCameraResourcePtr& camera,
            const QnUuid& rectUuid,
            const QRectF& rect)
        {
            if (camera != m_camera)
                return;

            emit regionAddedOrChanged(rectUuid, rect);
        });

    connect(qnMetadataAnalyticsController,
        &MetadataAnalyticsController::rectangleRemoved,
        this,
        [this](const QnVirtualCameraResourcePtr& camera, const QnUuid& rectUuid)
        {
            if (camera != m_camera)
                return;

            emit regionRemoved(rectUuid);
        });

}

} // namespace desktop
} // namespace client
} // namespace nx
