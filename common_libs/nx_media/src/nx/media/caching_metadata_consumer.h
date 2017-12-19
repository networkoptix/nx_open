#pragma once

#include "abstract_metadata_consumer.h"

namespace nx {
namespace media {

class CachingMetadataConsumer: public AbstractMetadataConsumer
{
    using base_type = AbstractMetadataConsumer;

public:
    CachingMetadataConsumer(MetadataType metadataType);
    virtual ~CachingMetadataConsumer() override;

    QnAbstractCompressedMetadataPtr metadata(qint64 timestamp, int channel) const;
    QList<QnAbstractCompressedMetadataPtr> metadataRange(
        qint64 startTimestamp, qint64 endTimestamp, int channel, int maximumCount = -1) const;

protected:
    virtual void processMetadata(const QnAbstractCompressedMetadataPtr& metadata) override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace media
} // namespace nx
