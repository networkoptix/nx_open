#pragma once

#include <chrono>

#include "abstract_metadata_consumer.h"

namespace nx::media {

class CachingMetadataConsumer: public AbstractMetadataConsumer
{
    using base_type = AbstractMetadataConsumer;

public:
    CachingMetadataConsumer(MetadataType metadataType);
    virtual ~CachingMetadataConsumer() override;

    size_t cacheSize() const;
    void setCacheSize(size_t cacheSize);

    QnAbstractCompressedMetadataPtr metadata(
        std::chrono::microseconds timestamp, int channel) const;
    QList<QnAbstractCompressedMetadataPtr> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel,
        int maximumCount) const;

protected:
    virtual void processMetadata(const QnAbstractCompressedMetadataPtr& metadata) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::media
