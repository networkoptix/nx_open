// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <nx/media/meta_data_packet.h>

namespace nx::analytics {

constexpr std::chrono::milliseconds kMetadataCacheDuration(10000);
constexpr size_t kMetadataCacheSize = 1000;

class NX_VMS_COMMON_API AbstractMetadataConsumer
{
public:
    virtual ~AbstractMetadataConsumer();

    MetadataType metadataType() const;
    virtual void processMetadata(const QnConstAbstractCompressedMetadataPtr& metadata) = 0;

protected:
    AbstractMetadataConsumer(MetadataType metadataType);

private:
    const MetadataType m_metadataType;
};

using AbstractMetadataConsumerPtr = QSharedPointer<AbstractMetadataConsumer>;

} // namespace nx::analytics
