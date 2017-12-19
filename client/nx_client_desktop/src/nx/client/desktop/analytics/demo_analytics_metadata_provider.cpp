#include "demo_analytics_metadata_provider.h"

#include <nx/utils/random.h>
#include <utils/math/math.h>
#include <ini.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kDefaultObjectsNumber = 4;
static constexpr qint64 kDefaultDuration = 100;

static QRectF shiftedRect(const QRectF& rect, qint64 timestamp)
{
    const auto kSpeed = 0.025; //< Per second.

    QRectF result = rect;
    result.moveLeft(qMod(rect.x() + timestamp / 1000000.0 * kSpeed, 1.0 - rect.width()));
    return result;
}

} // namespace

class DemoAnalyticsMetadataProvider::Private
{
public:
    int objectsNumber = kDefaultObjectsNumber;
    qint64 duration = kDefaultDuration;

    struct Object
    {
        QnUuid id;
        QRectF initialGeometry;
    };
    QVector<Object> objects;

public:
    void createObjects()
    {
        objects.reserve(objectsNumber);

        for (int i = 0; i < objectsNumber; ++i)
        {
            const auto w = utils::random::number(0.1, 0.4);
            const auto h = utils::random::number(0.1, 0.4);

            objects.append(Object{
                QnUuid::createUuid(),
                QRectF(
                    utils::random::number(0.0, 1.0 - w),
                    utils::random::number(0.0, 1.0 - h),
                    w,
                    h)});
        }
    }
};

DemoAnalyticsMetadataProvider::DemoAnalyticsMetadataProvider():
    d(new Private())
{
}

common::metadata::DetectionMetadataPacketPtr DemoAnalyticsMetadataProvider::metadata(
    qint64 timestamp, int /*channel*/) const
{
    if (d->objectsNumber <= 0)
        return common::metadata::DetectionMetadataPacketPtr();

    if (d->objects.isEmpty())
        d->createObjects();

    const common::metadata::DetectionMetadataPacketPtr metadata(
        new common::metadata::DetectionMetadataPacket());

    metadata->timestampUsec = timestamp * 1000;
    metadata->durationUsec = d->duration * 1000;

    for (int i = 0; i < d->objectsNumber; ++i)
    {
        common::metadata::DetectedObject object;
        object.objectId = d->objects[i].id;
        object.labels.emplace_back(
            common::metadata::Attribute{lit("Object %1").arg(i), QString()});
        object.boundingBox = shiftedRect(d->objects[i].initialGeometry, timestamp);

        metadata->objects.push_back(object);
    }

    return metadata;
}

bool DemoAnalyticsMetadataProviderFactory::supportsAnalytics(
    const QnResourcePtr& /*resource*/) const
{
    return ini().demoAnalyticsDriver;
}

core::AbstractAnalyticsMetadataProviderPtr
    DemoAnalyticsMetadataProviderFactory::createMetadataProvider(
        const QnResourcePtr& resource) const
{
    if (!supportsAnalytics(resource))
        return core::AbstractAnalyticsMetadataProviderPtr();

    return core::AbstractAnalyticsMetadataProviderPtr(new DemoAnalyticsMetadataProvider());
}

} // namespace desktop
} // namespace client
} // namespace nx
