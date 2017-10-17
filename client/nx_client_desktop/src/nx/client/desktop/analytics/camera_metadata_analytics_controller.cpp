#include "camera_metadata_analytics_controller.h"

#include <set>

#include <core/resource/camera_resource.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace client {
namespace desktop {

void MetadataAnalyticsController::gotMetadataPacket(
    const QnVirtualCameraResourcePtr& camera,
    const QnCompressedMetadataPtr& metadata)
{
    if (!camera)
        return;

    bool metadataIsOk = metadata
        && metadata->dataType == QnAbstractMediaData::DataType::GENERIC_METADATA
        && metadata->metadataType == MetadataType::ObjectDetection;

    if (!metadataIsOk)
        return;

    QnObjectDetectionMetadata objectDetectionMetadata;
    objectDetectionMetadata.deserialize(metadata);

    handleObjectDetectionMetadata(camera, objectDetectionMetadata);
}

QRectF MetadataAnalyticsController::toQRectF(const QnRect& rect)
{
    QRectF rectf;
    rectf.setRect(
        rect.topLeft.x,
        rect.topLeft.y,
        rect.bottomRight.x - rect.topLeft.x,
        rect.bottomRight.y - rect.topLeft.y);

    return rectf;
}

void MetadataAnalyticsController::handleObjectDetectionMetadata(
        const QnVirtualCameraResourcePtr& camera,
        const QnObjectDetectionMetadata& objectDetectionMetadata)
{
    std::map<QnUuid, QRectF> rectangles;
    auto& prevRectangles = m_rectMap[camera->getId()];
    auto detectedObjects = objectDetectionMetadata.detectedObjects;

    for (const auto& obj: detectedObjects)
        rectangles[obj.objectId] = toQRectF(obj.boundingBox);


    for (const auto& uuid: prevRectangles)
    {
        auto found = rectangles.find(uuid);

        if (found == rectangles.end())
            emit rectangleRemoved(camera, uuid);
    }

    prevRectangles.clear();
    for (const auto& item: rectangles)
    {
        prevRectangles.insert(item.first);
        emit rectangleAddedOrChanged(camera, item.first, item.second);
    }
}

} // namespace desktop
} // namespace client
} // namespace nx
