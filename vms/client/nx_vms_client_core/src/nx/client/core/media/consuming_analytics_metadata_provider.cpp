#include "consuming_analytics_metadata_provider.h"

#include <analytics/common/object_detection_metadata.h>
#include <core/resource/camera_resource.h>
#include <nx/media/caching_metadata_consumer.h>
#include <nx/fusion/serialization/ubjson.h>

namespace nx::vms::client::core {

using namespace std::chrono;
using namespace nx::common::metadata;

class ConsumingAnalyticsMetadataProvider::Private
{
public:
    QSharedPointer<media::CachingMetadataConsumer> metadataConsumer{
        new media::CachingMetadataConsumer(MetadataType::ObjectDetection)};
};

ConsumingAnalyticsMetadataProvider::ConsumingAnalyticsMetadataProvider():
    d(new Private())
{
}

ConsumingAnalyticsMetadataProvider::~ConsumingAnalyticsMetadataProvider()
{
}

DetectionMetadataPacketPtr ConsumingAnalyticsMetadataProvider::metadata(
    microseconds timestamp, int channel) const
{
    const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(
        d->metadataConsumer->metadata(timestamp, channel));

    return fromMetadataPacket(compressedMetadata);
}

QList<DetectionMetadataPacketPtr> ConsumingAnalyticsMetadataProvider::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel,
    int maximumCount) const
{
    const auto& metadataList = d->metadataConsumer->metadataRange(
        startTimestamp, endTimestamp, channel, maximumCount);

    QList<DetectionMetadataPacketPtr> result;
    for (const auto& metadata: metadataList)
    {
        const auto compressedMetadata =
            std::dynamic_pointer_cast<QnCompressedMetadata>(metadata);
        result.append(fromMetadataPacket(compressedMetadata));
    }
    return result;
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
