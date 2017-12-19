#include "consuming_analytics_metadata_provider.h"

#include <analytics/common/object_detection_metadata.h>
#include <core/resource/camera_resource.h>
#include <nx/media/caching_metadata_consumer.h>
#include <nx/fusion/serialization/ubjson.h>

namespace nx {
namespace client {
namespace core {

class ConsumingAnalyticsMetadataProvider::Private
{
public:
    Private();

    QSharedPointer<media::CachingMetadataConsumer> metadataConsumer;
};

ConsumingAnalyticsMetadataProvider::Private::Private():
    metadataConsumer(new media::CachingMetadataConsumer(MetadataType::ObjectDetection))
{
}

ConsumingAnalyticsMetadataProvider::ConsumingAnalyticsMetadataProvider():
    d(new Private())
{
}

common::metadata::DetectionMetadataPacketPtr ConsumingAnalyticsMetadataProvider::metadata(
    qint64 timestamp, int channel) const
{
    const auto compressedMetadata = std::dynamic_pointer_cast<QnCompressedMetadata>(
        d->metadataConsumer->metadata(timestamp, channel));

    return common::metadata::fromMetadataPacket(compressedMetadata);
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

} // namespace core
} // namespace client
} // namespace nx
