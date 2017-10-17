#pragma once

#include <QtCore/QRectF>
#include <QtCore/QPointF>
#include <QtCore/QSizeF>
#include <QtCore/QElapsedTimer>

#include <nx/client/desktop/analytics/drivers/abstract_analytics_driver.h>

#include <analytics/common/object_detection_metadata.h>

namespace nx {
namespace client {
namespace desktop {

class DemoAnalyticsDriver: public AbstractAnalyticsDriver
{
    using base_type = AbstractAnalyticsDriver;

public:
    explicit DemoAnalyticsDriver(QObject* parent = nullptr);
    virtual ~DemoAnalyticsDriver() override;

private:
    void tick();
    void addRegionIfNeeded();

private:
    struct DemoRegion
    {
        QnUuid id;
        QRectF geometry;
        QPointF offsetPerTick;
        QSizeF resizePerTick;

        void tick();
        bool isValid() const;
    };

    QList<DemoRegion> m_regions;

    QElapsedTimer m_elapsed;
    std::vector<QnObjectDetectionMetadataTrack> m_track;
};

} // namespace desktop
} // namespace client
} // namespace nx