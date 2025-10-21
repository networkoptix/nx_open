// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "consuming_analytics_metadata_provider.h"

#include <analytics/common/object_metadata.h>
#include <core/resource/camera_resource.h>
#include <nx/fusion/serialization/ubjson.h>
#include <nx/analytics/caching_metadata_consumer.h>
#include <nx/media/ini.h>

namespace nx::vms::client::core {

using namespace std::chrono;
using namespace nx::common::metadata;

class ConsumingAnalyticsMetadataProvider::Private
{
public:
    QSharedPointer<nx::analytics::CachingMetadataConsumer<ObjectMetadataPacketPtr>> metadataConsumer{
        new nx::analytics::CachingMetadataConsumer<ObjectMetadataPacketPtr>(
            MetadataType::ObjectDetection, nx::media::ini().metadataCacheSize)};
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

std::list<ObjectMetadataPacketPtr> ConsumingAnalyticsMetadataProvider::metadataRange(
    microseconds startTimestamp,
    microseconds endTimestamp,
    int channel) const
{
    return d->metadataConsumer->metadataRange(
        startTimestamp,
        endTimestamp,
        channel);
}

QSharedPointer<nx::analytics::AbstractMetadataConsumer>
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
