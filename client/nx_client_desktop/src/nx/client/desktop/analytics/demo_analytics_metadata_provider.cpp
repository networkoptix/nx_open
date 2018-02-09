#include "demo_analytics_metadata_provider.h"

#include <chrono>

#include <QtCore/QFile>

#include <nx/fusion/model_functions.h>
#include <nx/utils/random.h>
#include <utils/math/math.h>
#include <ini.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr qint64 kDefaultDuration = 100;

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

struct DemoAnalyticsObject: common::metadata::DetectedObject
{
    QPointF movementSpeed;

    DemoAnalyticsObject animated(std::chrono::milliseconds dt)
    {
        // Ping-pong movement animation.

        auto movedCoordinate = [dt](qreal value, qreal speed, qreal space)
            {
                value = std::abs(value + speed * (dt.count() / 1000.0));

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
    qint64 duration = kDefaultDuration;

    QVector<DemoAnalyticsObject> objects;
    std::chrono::milliseconds startTimestamp = std::chrono::milliseconds::zero();

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
                    object.objectTypeId = QnUuid::createUuid();

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
                common::metadata::Attribute{lit("Object %1").arg(i), QString()});
            object.movementSpeed = QPointF(1.0, 1.0);

            objects.append(object);
        }
    }
};

DemoAnalyticsMetadataProvider::DemoAnalyticsMetadataProvider():
    d(new Private())
{
}

common::metadata::DetectionMetadataPacketPtr DemoAnalyticsMetadataProvider::metadata(
    qint64 timestampUs, int /*channel*/) const
{
    using namespace std::chrono;

    if (d->objectsCount <= 0)
        return common::metadata::DetectionMetadataPacketPtr();

    if (d->objects.isEmpty())
        d->createObjects();

    const common::metadata::DetectionMetadataPacketPtr metadata(
        new common::metadata::DetectionMetadataPacket());

    const auto precision = ini().demoAnalyticsProviderTimestampPrecisionUs;
    if (precision > 0)
        timestampUs -= timestampUs % precision;

    const auto timestamp = duration_cast<milliseconds>(microseconds(timestampUs));
    if (d->startTimestamp == milliseconds::zero())
        d->startTimestamp = timestamp;
    const auto dt = timestamp - d->startTimestamp;

    metadata->timestampUsec = timestampUs;
    metadata->durationUsec = d->duration * 1000;

    for (auto& object: d->objects)
        metadata->objects.push_back(object.animated(dt));

    return metadata;
}

QList<common::metadata::DetectionMetadataPacketPtr> DemoAnalyticsMetadataProvider::metadataRange(
    qint64 startTimestamp, qint64 endTimestamp, int channel, int maximumCount) const
{
    const auto precision = ini().demoAnalyticsProviderTimestampPrecisionUs;
    if (precision > 0)
    {
        startTimestamp = startTimestamp - startTimestamp % precision;
        endTimestamp -= endTimestamp % precision;
    }

    QList<common::metadata::DetectionMetadataPacketPtr> result;
    if (maximumCount > 0)
        result.append(metadata(startTimestamp, channel));
    if (maximumCount > 1)
        result.append(metadata(endTimestamp, channel));
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

} // namespace desktop
} // namespace client
} // namespace nx
