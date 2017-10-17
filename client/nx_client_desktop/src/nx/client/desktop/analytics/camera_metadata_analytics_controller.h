#pragma once

#include <map>
#include <QRectF>

#include <nx/utils/singleton.h>
#include <analytics/common/abstract_metadata.h>
#include <analytics/common/object_detection_metadata.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace desktop {

class MetadataAnalyticsController:
    public QObject,
    public Singleton<MetadataAnalyticsController>
{
    Q_OBJECT

public:
    void gotMetadataPacket(
        const QnVirtualCameraResourcePtr& camera, const QnCompressedMetadataPtr& metadata);

signals:
    void rectangleAddedOrChanged(
        const QnVirtualCameraResourcePtr& camera, const QnUuid& rectId, const QRectF& rect);

    void rectangleRemoved(const QnVirtualCameraResourcePtr& camera, const QnUuid& rectId);

private:
    QRectF toQRectF(const QnRect& rect);

    void handleObjectDetectionMetadata(
        const QnVirtualCameraResourcePtr& camera,
        const QnObjectDetectionMetadata& objectDetectionMetadata);

private:
    std::map<QnUuid, std::set<QnUuid>> m_rectMap;
};

} // namespace desktop
} // namespace client
} // namespace nx

#define qnMetadataAnalyticsController nx::client::desktop::MetadataAnalyticsController::instance()
