#include "caching_metadata_consumer.h"

#include <QtCore/QCache>
#include <QtCore/QVector>

namespace nx {
namespace media {

class CachingMetadataConsumer::Private
{
public:
    // So far metadata packets goes strongly with the corresponding frame packets, so when we're
    // rendering a frame we have its metadata saved. If this is changed one day (e.g. consumer
    // gets metadata for multiple frames at once) then we need the real cache implementation.
    QVector<QnAbstractCompressedMetadataPtr> cachedMetadata;
};

CachingMetadataConsumer::CachingMetadataConsumer(MetadataType metadataType):
    base_type(metadataType),
    d(new Private())
{
}

CachingMetadataConsumer::~CachingMetadataConsumer()
{
}

QnAbstractCompressedMetadataPtr CachingMetadataConsumer::metadata(qint64 timestamp, int channel)
{
    if (channel >= d->cachedMetadata.size())
        return QnAbstractCompressedMetadataPtr();

    const auto& metadata = d->cachedMetadata[channel];
    if (metadata && metadata->containTime(timestamp))
        return metadata;

    return QnAbstractCompressedMetadataPtr();
}

void CachingMetadataConsumer::processMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    const auto channel = static_cast<int>(metadata->channelNumber);
    if (d->cachedMetadata.size() <= channel)
        d->cachedMetadata.resize(channel + 1);

    d->cachedMetadata[channel] = metadata;
}

} // namespace media
} // namespace nx
