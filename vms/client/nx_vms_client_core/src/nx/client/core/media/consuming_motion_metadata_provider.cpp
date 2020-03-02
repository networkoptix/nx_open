#include "consuming_motion_metadata_provider.h"

#include <nx/media/caching_metadata_consumer.h>

namespace nx::vms::client::core {

class ConsumingMotionMetadataProvider::Private
{
public:
    Private();

    QSharedPointer<media::CachingMetadataConsumer<MetaDataV1Ptr>> metadataConsumer;
};

ConsumingMotionMetadataProvider::Private::Private():
    metadataConsumer(new media::CachingMetadataConsumer<MetaDataV1Ptr>(MetadataType::Motion))
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

QSharedPointer<media::AbstractMetadataConsumer>
    ConsumingMotionMetadataProvider::metadataConsumer() const
{
    return d->metadataConsumer;
}

} // namespace nx::vms::client::core
