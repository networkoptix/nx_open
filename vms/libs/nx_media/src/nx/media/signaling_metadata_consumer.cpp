#include "signaling_metadata_consumer.h"

namespace nx {
namespace media {

SignalingMetadataConsumer::SignalingMetadataConsumer(MetadataType metadataType, QObject* parent):
    QObject(parent),
    AbstractMetadataConsumer(metadataType)
{
}

SignalingMetadataConsumer::~SignalingMetadataConsumer()
{
}

void SignalingMetadataConsumer::processMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    emit metadataReceived(metadata);
}

} // namespace media
} // namespace nx
