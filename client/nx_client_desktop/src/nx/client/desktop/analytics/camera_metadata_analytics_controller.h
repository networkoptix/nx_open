#pragma once

#include <map>
#include <QRectF>

#include <nx/utils/singleton.h>
#include <analytics/common/object_detection_metadata.h>
#include <core/resource/resource_fwd.h>
#include <nx/streaming/media_data_packet.h>

namespace nx {
namespace client {
namespace desktop {

class MetadataAnalyticsController:
    public QObject,
    public Singleton<MetadataAnalyticsController>
{
    Q_OBJECT

public:
    void gotMetadata(
        const QnResourcePtr& resource,
        const common::metadata::DetectionMetadataPacketPtr& metadata);
    void gotFrame(const QnResourcePtr& resource, qint64 timestampUs);

signals:
    void frameReceived(const QnResourcePtr& resource, qint64 timestampUs);

    void rectangleAddedOrChanged(const QnResourcePtr& resource, const QnUuid& rectId,
        const QRectF& rect);

    void rectangleRemoved(const QnResourcePtr& resource, const QnUuid& rectId);

private:
    std::map<QnUuid, std::set<QnUuid>> m_rectMap;
};

} // namespace desktop
} // namespace client
} // namespace nx

#define qnMetadataAnalyticsController nx::client::desktop::MetadataAnalyticsController::instance()
