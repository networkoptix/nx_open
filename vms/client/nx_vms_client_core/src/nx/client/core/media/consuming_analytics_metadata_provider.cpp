#include "consuming_analytics_metadata_provider.h"

#include <analytics/common/object_metadata.h>
#include <core/resource/camera_resource.h>
#include <nx/media/caching_metadata_consumer.h>
#include <nx/fusion/serialization/ubjson.h>

namespace nx::vms::client::core {

using namespace std::chrono;
using namespace nx::common::metadata;

class ConsumingAnalyticsMetadataProvider::Private
{
public:
    QSharedPointer<media::CachingMetadataConsumer<ObjectMetadataPacketPtr>> metadataConsumer{
        new media::CachingMetadataConsumer<ObjectMetadataPacketPtr>(
            MetadataType::ObjectDetection)};
};

ConsumingAnalyticsMetadataProvider::ConsumingAnalyticsMetadataProvider():
    d(new Private())
{
}

ConsumingAnalyticsMetadataProvider::~ConsumingAnalyticsMetadataProvider()
{
}

ObjectMetadataPacketPtr ConsumingAnalyticsMetadataProvider::metadata(
    microseconds timestamp, int channel) const
{
    return d->metadataConsumer->metadata(timestamp, channel);
}

QList<ObjectMetadataPacketPtr> ConsumingAnalyticsMetadataProvider::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    int maximumCount) const
{
    return d->metadataConsumer->metadataRange(
        startTimestamp,
        endTimestamp,
        channel,
        media::CachingMetadataConsumer<ObjectMetadataPacketPtr>::PickingPolicy::TakeFirst,
        maximumCount);
}

QSharedPointer<media::AbstractMetadataConsumer>
    ConsumingAnalyticsMetadataProvider::metadataConsumer() const
{
    return d->metadataConsumer;
}

bool ConsumingAnalyticsMetadataProviderFactory::supportsAnalytics(
    const QnResourcePtr& resource) const
{
    return !resource.dynamicCast<QnVirtualCameraResource>().isNull();
}

AbstractAnalyticsMetadataProviderPtr
    ConsumingAnalyticsMetadataProviderFactory::createMetadataProvider(
        const QnResourcePtr& resource) const
{
    if (!supportsAnalytics(resource))
        return AbstractAnalyticsMetadataProviderPtr();

    return AbstractAnalyticsMetadataProviderPtr(new ConsumingAnalyticsMetadataProvider());
}

} // namespace nx::vms::client::core
