#include "camera_metadata_analytics_controller.h"

#include <set>

#include <core/resource/camera_resource.h>
#include <analytics/common/object_metadata.h>
#include <nx/fusion/fusion/fusion.h>
#include <nx/fusion/serialization/ubjson.h>

namespace nx::vms::client::desktop {

void MetadataAnalyticsController::gotMetadata(
    const QnResourcePtr& resource,
    const nx::common::metadata::ObjectMetadataPacketPtr& metadataPacket)
{
    std::map<QnUuid, QRectF> rectangles;
    auto& prevRectangles = m_rectMap[resource->getId()];

    for (const auto& objectMetadata: metadataPacket->objectMetadataList)
        rectangles[objectMetadata.trackId] = QRectF(objectMetadata.boundingBox);

    for (const auto& uuid: prevRectangles)
    {
        const auto found = rectangles.find(uuid);

        if (found == rectangles.end())
            emit rectangleRemoved(resource, uuid);
    }

    prevRectangles.clear();
    for (const auto& item: rectangles)
    {
        prevRectangles.insert(item.first);
        emit rectangleAddedOrChanged(resource, item.first, item.second);
    }
}

void MetadataAnalyticsController::gotFrame(const QnResourcePtr& resource, qint64 timestampUs)
{
    emit frameReceived(resource, timestampUs);
}

} // namespace nx::vms::client::desktop
