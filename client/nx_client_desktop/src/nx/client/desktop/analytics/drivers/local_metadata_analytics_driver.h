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
    explicit LocalMetadataAnalyticsDriver(const QnResourcePtr& resource, QObject* parent = nullptr);

    static bool supportsAnalytics(const QnResourcePtr& resource);

private:
    const QnResourcePtr m_resource;
    std::vector<nx::common::metadata::DetectionMetadataTrack> m_track;
    std::vector<nx::common::metadata::DetectionMetadataTrack>::const_iterator m_currentFrame;
};

} // namespace desktop
} // namespace client
} // namespace nx
