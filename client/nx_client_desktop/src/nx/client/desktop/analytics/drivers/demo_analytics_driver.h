#pragma once

#include <QtCore/QRectF>
#include <QtCore/QPointF>
#include <QtCore/QSizeF>

#include <nx/client/desktop/analytics/drivers/abstract_analytics_driver.h>

namespace nx {
namespace client {
namespace desktop {

class DemoAnalyticsDriver: public AbstractAnalyticsDriver
{
    using base_type = AbstractAnalyticsDriver;

public:
    explicit DemoAnalyticsDriver(QObject* parent = nullptr);

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
};

} // namespace desktop
} // namespace client
} // namespace nx