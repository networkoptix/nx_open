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

struct DemoAnalyticsObjectMetadata: ObjectMetadata
{
    QPointF movementSpeed;

    DemoAnalyticsObjectMetadata animated(microseconds dt)
    {
        // Ping-pong movement animation.

        auto movedCoordinate = [dt](qreal value, qreal speed, qreal space)
            {
                value = std::abs(value + speed * (dt.count() / 1000000.0));

                int reflections = static_cast<int>(value / space);
                value -= space * reflections;

                return (reflections % 2 == 1) ? space - value : value;
            };

        DemoAnalyticsObjectMetadata animatedObjectMetadata = *this;
        animatedObjectMetadata.boundingBox.moveTo(
            movedCoordinate(boundingBox.x(), movementSpeed.x(), 1.0 - boundingBox.width()),
            movedCoordinate(boundingBox.y(), movementSpeed.y(), 1.0 - boundingBox.height()));
        return animatedObjectMetadata;
    }
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(DemoAnalyticsObjectMetadata, (json),
    ObjectMetadata_Fields (movementSpeed));
} // namespace

class DemoAnalyticsMetadataProvider::Private
{
public:
    int objectMetadataCount = 0;

    QVector<DemoAnalyticsObjectMetadata> objectMetadataList;
    microseconds startTimestamp = 0us;

public:
    Private():
        objectMetadataCount(ini().demoAnalyticsProviderObjectsCount)
    {
    }

    void createObjectMetadata()
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

            objectMetadataList = QJson::deserialized<decltype(objectMetadataList)>(jsonData);

            for (auto& objectMetadata: objectMetadataList)
            {
                if (objectMetadata.trackId.isNull())
                    objectMetadata.trackId = QnUuid::createUuid();

                if (objectMetadata.objectTypeId.isNull())
                    objectMetadata.objectTypeId = QnUuid::createUuid().toString();

                if (objectMetadata.boundingBox.isNull())
                    objectMetadata.boundingBox = randomRect();
            }

            return;
        }

        objectMetadataList.reserve(objectMetadataCount);

        for (int i = 0; i < objectMetadataCount; ++i)
        {
            DemoAnalyticsObjectMetadata objectMetadata;
            objectMetadata.trackId = QnUuid::createUuid();
            objectMetadata.boundingBox = randomRect();
            objectMetadata.attributes.emplace_back(
                nx::common::metadata::Attribute{lit("Object %1").arg(i), QString()});
            objectMetadata.movementSpeed = QPointF(1.0, 1.0);

            objectMetadataList.append(objectMetadata);
        }
    }
};

DemoAnalyticsMetadataProvider::DemoAnalyticsMetadataProvider():
    d(new Private())
{
}

ObjectMetadataPacketPtr DemoAnalyticsMetadataProvider::metadata(
    microseconds timestamp, int /*channel*/) const
{
    if (d->objectMetadataCount <= 0)
        return {};

    if (d->objectMetadataList.isEmpty())
        d->createObjectMetadata();

    const auto metadata = std::make_shared<ObjectMetadataPacket>();

    const microseconds precision{ini().demoAnalyticsProviderTimestampPrecisionUs};
    if (precision > 0us)
        timestamp -= timestamp % precision;

    if (d->startTimestamp == 0us)
        d->startTimestamp = timestamp;
    const auto dt = timestamp - d->startTimestamp;

    metadata->timestampUs = timestamp.count();

    for (auto& objectMetadata: d->objectMetadataList)
        metadata->objectMetadataList.push_back(objectMetadata.animated(dt));

    return metadata;
}

QList<ObjectMetadataPacketPtr> DemoAnalyticsMetadataProvider::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    int maximumCount) const
{
    constexpr microseconds kMinPrecision = 100000us;

    const auto precision = std::max(
        microseconds(ini().demoAnalyticsProviderTimestampPrecisionUs), kMinPrecision);
    startTimestamp = startTimestamp - startTimestamp % precision;

    QList<ObjectMetadataPacketPtr> result;
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
