#include "proxy_analytics_driver.h"

#include <nx/client/desktop/analytics/camera_metadata_analytics_controller.h>

namespace nx {
namespace client {
namespace desktop {

ProxyAnalyticsDriver::ProxyAnalyticsDriver(const QnResourcePtr& resource,
    QObject* parent)
    :
    base_type(parent),
    m_resource(resource)
{
    connect(qnMetadataAnalyticsController,
        &MetadataAnalyticsController::rectangleAddedOrChanged,
        this,
        [this](const QnResourcePtr& resource,
            const QnUuid& rectUuid,
            const QRectF& rect)
        {
            if (resource != m_resource)
                return;

            emit regionAddedOrChanged(rectUuid, rect);
        });

    connect(qnMetadataAnalyticsController,
        &MetadataAnalyticsController::rectangleRemoved,
        this,
        [this](const QnResourcePtr& resource, const QnUuid& rectUuid)
        {
            if (resource != m_resource)
                return;

            emit regionRemoved(rectUuid);
        });

}

} // namespace desktop
} // namespace client
} // namespace nx
