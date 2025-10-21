// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <list>

#include <QtCore/QScopedPointer>

#include "abstract_metadata_consumer.h"

namespace nx::analytics {

static constexpr int kMetadataCashSize = 1000;

template<typename T>
class NX_VMS_COMMON_API CachingMetadataConsumer: public AbstractMetadataConsumer
{
    using base_type = AbstractMetadataConsumer;

public:
    CachingMetadataConsumer(MetadataType metadataType, int cashSize);
    virtual ~CachingMetadataConsumer() override;

    size_t cacheSize() const;
    void setCacheSize(size_t cacheSize);

    T metadata(
        std::chrono::microseconds timestamp, int channel) const;

    std::list<T> metadataRange(
        std::chrono::microseconds startTimestamp,
        std::chrono::microseconds endTimestamp,
        int channel) const;

    void processMetadata(const QnConstAbstractCompressedMetadataPtr& metadata) override;

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::analytics
