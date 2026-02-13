// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "abstract_metadata_consumer.h"

namespace nx::analytics {

/**
 * Consumer, that is supposed to be used for export and to utilize for storing data
 *  ObjectMetadataCache for Object detection and MetadataCache<TMetadata> for motion and others.
 *
 * @tparam TMetadata Metadata type to be handled.
 */
template<typename TMetadata>
class NX_VMS_COMMON_API ExportCachingMetadataConsumer: public AbstractMetadataConsumer
{
    using base_type = AbstractMetadataConsumer;

public:
    ExportCachingMetadataConsumer(
        const MetadataType metadataType,
        const std::chrono::milliseconds& cacheSize);

    virtual ~ExportCachingMetadataConsumer() override;

    std::chrono::milliseconds cacheDuration() const;
    void setCacheDuration(const std::chrono::milliseconds& cacheDuration);

    std::vector<TMetadata> metadata(
        const std::chrono::milliseconds& timestamp, const int channel) const;

    virtual void processMetadata(const QnConstAbstractCompressedMetadataPtr& metadata) override;

private:
    class Private;
    const std::unique_ptr<Private> d;
};

} // namespace nx::analytics
