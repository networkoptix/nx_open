#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/analytics/drivers/proxy_analytics_driver.h>
#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace client {
namespace desktop {

class LocalMetadataAnalyticsDriver: public ProxyAnalyticsDriver
{
    using base_type = ProxyAnalyticsDriver;

public:
    explicit LocalMetadataAnalyticsDriver(QObject* parent = nullptr);
    explicit LocalMetadataAnalyticsDriver(const QnResourcePtr& resource, QObject* parent = nullptr);

    bool loadTrack(const QnResourcePtr& resource);
    QRectF zoomRectFor(const QnUuid& regionId, qint64 timestampUs) const;

    static bool supportsAnalytics(const QnResourcePtr& resource);

private:
    QnObjectDetectionMetadata findMetadata(qint64 timestampUs) const;

private:
    const QnResourcePtr m_resource;
    std::vector<QnObjectDetectionMetadataTrack> m_track;
};

} // namespace desktop
} // namespace client
} // namespace nx
