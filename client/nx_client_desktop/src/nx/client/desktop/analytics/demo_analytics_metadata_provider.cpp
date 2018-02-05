#include "demo_analytics_metadata_provider.h"

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
    int objectsCount = 0;
    qint64 duration = kDefaultDuration;

    QVector<common::metadata::DetectedObject> objects;

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
            common::metadata::DetectedObject object;
            object.objectId = QnUuid::createUuid();
            object.boundingBox = randomRect();
            object.labels.emplace_back(
                common::metadata::Attribute{lit("Object %1").arg(i), QString()});

            objects.append(object);
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
    if (d->objectsCount <= 0)
        return common::metadata::DetectionMetadataPacketPtr();

    if (d->objects.isEmpty())
        d->createObjects();

    const common::metadata::DetectionMetadataPacketPtr metadata(
        new common::metadata::DetectionMetadataPacket());

    const auto precision = ini().demoAnalyticsProviderTimestampPrecisionUs;
    if (precision > 0)
        timestamp -= timestamp % precision;

    metadata->timestampUsec = timestamp;
    metadata->durationUsec = d->duration * 1000;

    for (const auto& object: d->objects)
    {
        auto movedObject = object;
        movedObject.boundingBox = shiftedRect(object.boundingBox, timestamp);
        metadata->objects.push_back(movedObject);
    }

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
