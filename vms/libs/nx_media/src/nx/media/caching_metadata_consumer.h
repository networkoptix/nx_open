// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QScopedPointer>

#include "abstract_metadata_consumer.h"

namespace nx::media {

template<typename T>
class CachingMetadataConsumer: public AbstractMetadataConsumer
{
    using base_type = AbstractMetadataConsumer;

public:
    CachingMetadataConsumer(MetadataType metadataType);
    virtual ~CachingMetadataConsumer() override;

    size_t cacheSize() const;
    void setCacheSize(size_t cacheSize);

    T metadata(
        std::chrono::microseconds timestamp, int channel) const;

    enum class PickingPolicy
    {
        TakeFirst,
        TakeLast,
    };
    QList<T> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel,
        PickingPolicy pickingPolicy,
        int maximumCount) const;

protected:
    virtual void processMetadata(const QnAbstractCompressedMetadataPtr& metadata) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::media
