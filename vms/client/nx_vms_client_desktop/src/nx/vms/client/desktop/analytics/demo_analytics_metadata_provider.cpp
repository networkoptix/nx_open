#include "demo_analytics_metadata_provider.h"

#include <chrono>

#include <QtCore/QFile>

#include <nx/vms/client/desktop/ini.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/random.h>
#include <utils/math/math.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace nx::common::metadata;

namespace {

static QRectF randomRect()
{
    const auto w = utils::random::number(0.1, 0.4);
    const auto h = utils::random::number(0.1, 0.4);

    return QRectF(
        utils::random::number(0.0, 1.0 - w),
        utils::random::number(0.0, 1.0 - h),
        w,
        h);
}

struct DemoAnalyticsObject: DetectedObject
{
    QPointF movementSpeed;

    DemoAnalyticsObject animated(microseconds dt)
    {
        // Ping-pong movement animation.

        auto movedCoordinate = [dt](qreal value, qreal speed, qreal space)
            {
                value = std::abs(value + speed * (dt.count() / 1000000.0));

                int reflections = static_cast<int>(value / space);
                value -= space * reflections;

                return (reflections % 2 == 1) ? space - value : value;
            };

        DemoAnalyticsObject animatedObject = *this;
        animatedObject.boundingBox.moveTo(
            movedCoordinate(boundingBox.x(), movementSpeed.x(), 1.0 - boundingBox.width()),
            movedCoordinate(boundingBox.y(), movementSpeed.y(), 1.0 - boundingBox.height()));
        return animatedObject;
    }
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DemoAnalyticsObject, (json),
    DetectedObject_Fields (movementSpeed));
} // namespace

class DemoAnalyticsMetadataProvider::Private
{
public:
    int objectsCount = 0;

    QVector<DemoAnalyticsObject> objects;
    microseconds startTimestamp = 0us;

public:
    Private():
        objectsCount(ini().demoAnalyticsProviderObjectsCount)
    {
    }

    void createObjects()
    {
        const auto& descriptionsFileName =
            QString::fromUtf8(ini().demoAnalyticsProviderObjectDescriptionsFile);

        if (!descriptionsFileName.isEmpty())
        {
            QFile file(descriptionsFileName);
            if (!file.open(QFile::ReadOnly))
                return;

            const QByteArray& jsonData = file.readAll();
            file.close();

            objects = QJson::deserialized<decltype(objects)>(jsonData);

            for (auto& object: objects)
            {
                if (object.objectId.isNull())
                    object.objectId = QnUuid::createUuid();

                if (object.objectTypeId.isNull())
                    object.objectTypeId = QnUuid::createUuid().toString();

                if (object.boundingBox.isNull())
                    object.boundingBox = randomRect();
            }

            return;
        }

        objects.reserve(objectsCount);

        for (int i = 0; i < objectsCount; ++i)
        {
            DemoAnalyticsObject object;
            object.objectId = QnUuid::createUuid();
            object.boundingBox = randomRect();
            object.labels.emplace_back(
                nx::common::metadata::Attribute{lit("Object %1").arg(i), QString()});
            object.movementSpeed = QPointF(1.0, 1.0);

            objects.append(object);
        }
    }
};

DemoAnalyticsMetadataProvider::DemoAnalyticsMetadataProvider():
    d(new Private())
{
}

DetectionMetadataPacketPtr DemoAnalyticsMetadataProvider::metadata(
    microseconds timestamp, int /*channel*/) const
{
    if (d->objectsCount <= 0)
        return {};

    if (d->objects.isEmpty())
        d->createObjects();

    const auto metadata = std::make_shared<DetectionMetadataPacket>();

    const microseconds precision{ini().demoAnalyticsProviderTimestampPrecisionUs};
    if (precision > 0us)
        timestamp -= timestamp % precision;

    if (d->startTimestamp == 0us)
        d->startTimestamp = timestamp;
    const auto dt = timestamp - d->startTimestamp;

    metadata->timestampUsec = timestamp.count();

    for (auto& object: d->objects)
        metadata->objects.push_back(object.animated(dt));

    return metadata;
}

QList<DetectionMetadataPacketPtr> DemoAnalyticsMetadataProvider::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    int maximumCount) const
{
    constexpr microseconds kMinPrecision = 100000us;

    const auto precision = std::max(
        microseconds(ini().demoAnalyticsProviderTimestampPrecisionUs), kMinPrecision);
    startTimestamp = startTimestamp - startTimestamp % precision;

    QList<DetectionMetadataPacketPtr> result;
    for (int i = 0; i < maximumCount && startTimestamp <= endTimestamp; ++i)
    {
        result.append(metadata(startTimestamp, channel));
        startTimestamp += precision;
    }
    return result;
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

} // namespace nx::vms::client::desktop
