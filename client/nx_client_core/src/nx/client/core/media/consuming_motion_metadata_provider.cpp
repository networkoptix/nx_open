#include "consuming_motion_metadata_provider.h"

#include <nx/media/caching_metadata_consumer.h>

namespace nx {
namespace client {
namespace core {

class ConsumingMotionMetadataProvider::Private
{
public:
    Private();

    QSharedPointer<media::CachingMetadataConsumer> metadataConsumer;
};

ConsumingMotionMetadataProvider::Private::Private():
    metadataConsumer(new media::CachingMetadataConsumer(MetadataType::Motion))
{
}

ConsumingMotionMetadataProvider::ConsumingMotionMetadataProvider():
    d(new Private())
{
}

MetaDataV1Ptr ConsumingMotionMetadataProvider::metadata(const qint64 timestamp, int channel)
{
    return std::dynamic_pointer_cast<QnMetaDataV1>(
        d->metadataConsumer->metadata(timestamp, channel));
}

QSharedPointer<media::AbstractMetadataConsumer>
    ConsumingMotionMetadataProvider::metadataConsumer() const
{
    return d->metadataConsumer;
}

} // namespace core
} // namespace client
} // namespace nx
