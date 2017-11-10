#include "demo_analytics_driver.h"

#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QTimer>

#include <ini.h>

#include <nx/utils/random.h>

#include <nx/fusion/model_functions.h>

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

    m_elapsed.start();
}

DemoAnalyticsDriver::~DemoAnalyticsDriver()
{
    if (ini().externalMetadata)
    {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
        QFile file(dir.absoluteFilePath(lit("track.metadata")));
        file.open(QIODevice::WriteOnly);
        file.write(QJson::serialized(m_track));
        file.close();
    }
}

void DemoAnalyticsDriver::tick()
{
    QnObjectDetectionMetadataTrack track;
    track.timestampMs = m_elapsed.elapsed();

    for (auto region = m_regions.begin(); region != m_regions.end(); )
    {
        region->tick();
        if (region->isValid())
        {
            QnObjectDetectionInfo info;
            info.objectId = region->id;
            info.boundingBox = region->geometry;
            track.objects.push_back(info);

            emit regionAddedOrChanged(region->id, region->geometry);
            ++region;
        }
        else
        {
            emit regionRemoved(region->id);
            region = m_regions.erase(region);
        }
    }

    if (track.objects.empty() && !m_track.empty() && m_track.back().objects.empty())
        return;
    m_track.push_back(track);
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