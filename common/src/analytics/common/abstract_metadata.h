#pragma once

#include <nx/streaming/media_data_packet.h>

struct QnAbstractMetadata
{
    virtual ~QnAbstractMetadata() = default;

    virtual QnCompressedMetadataPtr serialize() const = 0;
    virtual bool deserialize(const QnConstCompressedMetadataPtr& data) = 0;
};

using QnAbstractMetadataPtr = std::shared_ptr<QnAbstractMetadata>;
