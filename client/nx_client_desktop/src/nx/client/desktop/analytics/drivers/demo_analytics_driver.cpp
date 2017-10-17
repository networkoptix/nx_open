#include "demo_analytics_driver.h"

#include <QtCore/QTimer>

#include <nx/utils/random.h>

namespace {

static constexpr int kRegionLimit = 12;

} // namespace

namespace nx {
namespace client {
namespace desktop {

DemoAnalyticsDriver::DemoAnalyticsDriver(QObject* parent):
    base_type(parent)
{
    auto moveTimer = new QTimer(this);
    moveTimer->setInterval(16);
    moveTimer->setSingleShot(false);
    connect(moveTimer, &QTimer::timeout, this, &DemoAnalyticsDriver::tick);
    moveTimer->start();

    auto addTimer = new QTimer(this);
    addTimer->setInterval(700);
    addTimer->setSingleShot(false);
    connect(addTimer, &QTimer::timeout, this, &DemoAnalyticsDriver::addRegionIfNeeded);
    addTimer->start();
}

void DemoAnalyticsDriver::tick()
{
    for (auto region = m_regions.begin(); region != m_regions.end(); )
    {
        region->tick();
        if (region->isValid())
        {
            emit regionAddedOrChanged(region->id, region->geometry);
            ++region;
        }
        else
        {
            emit regionRemoved(region->id);
            region = m_regions.erase(region);
        }
    }
}

void DemoAnalyticsDriver::addRegionIfNeeded()
{
    if (m_regions.size() >= kRegionLimit)
        return;

    DemoRegion region;
    region.id = QnUuid::createUuid();
    region.geometry.setLeft(-0.1);
    region.geometry.setTop(nx::utils::random::number(-0.5, 0.5));
    region.geometry.setWidth(nx::utils::random::number(0.05, 0.2));
    region.geometry.setHeight(nx::utils::random::number(0.05, 0.2));
    region.offsetPerTick.setX(nx::utils::random::number(0.001, 0.01));
    region.offsetPerTick.setY(nx::utils::random::number(-0.003, 0.003));
    region.resizePerTick.setWidth(nx::utils::random::number(-0.001, 0.001));
    region.resizePerTick.setHeight(nx::utils::random::number(-0.001, 0.001));

    m_regions.push_back(region);
    emit regionAddedOrChanged(region.id, region.geometry);
}

void DemoAnalyticsDriver::DemoRegion::tick()
{
    geometry.moveTo(geometry.topLeft() + offsetPerTick);

    QSizeF size(geometry.size() + resizePerTick);
    size.setWidth(qBound(0.05, size.width(), 0.9));
    size.setHeight(qBound(0.05, size.height(), 0.9));

    geometry.setSize(size);
}

bool DemoAnalyticsDriver::DemoRegion::isValid() const
{
    return QRectF(0, 0, 1, 1).intersects(geometry);
}

} // namespace desktop
} // namespace client
} // namespace nx