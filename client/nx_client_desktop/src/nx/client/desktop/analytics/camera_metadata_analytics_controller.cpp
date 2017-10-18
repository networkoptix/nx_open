#include "camera_metadata_analytics_controller.h"

#include <set>

#include <core/resource/camera_resource.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace client {
namespace desktop {

void MetadataAnalyticsController::gotMetadataPacket(
    const QnResourcePtr& resource,
    const QnCompressedMetadataPtr& metadata)
{
    if (!resource)
        return;

    const auto metadataIsOk = metadata
        && metadata->dataType == QnAbstractMediaData::DataType::GENERIC_METADATA
        && metadata->metadataType == MetadataType::ObjectDetection;

    if (!metadataIsOk)
        return;

    QnObjectDetectionMetadata objectDetectionMetadata;
    objectDetectionMetadata.deserialize(metadata);

    gotMetadata(resource, objectDetectionMetadata);
}

void MetadataAnalyticsController::gotMetadata(const QnResourcePtr& resource,
    const QnObjectDetectionMetadata& metadata)
{
    std::map<QnUuid, QRectF> rectangles;
    auto& prevRectangles = m_rectMap[resource->getId()];
    auto detectedObjects = metadata.detectedObjects;

    for (const auto& obj: detectedObjects)
        rectangles[obj.objectId] = obj.boundingBox.toRectF();

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

} // namespace desktop
} // namespace client
} // namespace nx
