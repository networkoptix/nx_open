// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "consuming_motion_metadata_provider.h"

#include <nx/analytics/caching_metadata_consumer.h>
#include <nx/media/ini.h>

namespace nx::vms::client::core {

class ConsumingMotionMetadataProvider::Private
{
public:
    Private();

    QSharedPointer<nx::analytics::CachingMetadataConsumer<MetaDataV1Ptr>> metadataConsumer;
};

ConsumingMotionMetadataProvider::Private::Private():
    metadataConsumer(new nx::analytics::CachingMetadataConsumer<MetaDataV1Ptr>(
        MetadataType::Motion, nx::media::ini().metadataCacheSize))
{
}

ConsumingMotionMetadataProvider::~ConsumingMotionMetadataProvider()
{
}

ConsumingMotionMetadataProvider::ConsumingMotionMetadataProvider():
    d(new Private())
{
}

MetaDataV1Ptr ConsumingMotionMetadataProvider::metadata(
    const std::chrono::microseconds timestamp, int channel) const
{
    return d->metadataConsumer->metadata(timestamp, channel);
}

QSharedPointer<nx::analytics::AbstractMetadataConsumer>
    ConsumingMotionMetadataProvider::metadataConsumer() const
{
    return d->metadataConsumer;
}

} // namespace nx::vms::client::core
