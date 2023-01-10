// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSharedPointer>

#include <nx/streaming/media_data_packet.h>

namespace nx {
namespace media {

class AbstractMetadataConsumer
{
public:
    virtual ~AbstractMetadataConsumer();

    MetadataType metadataType() const;
    virtual void processMetadata(const QnAbstractCompressedMetadataPtr& metadata) = 0;

protected:
    AbstractMetadataConsumer(MetadataType metadataType);

private:
    const MetadataType m_metadataType;
};

using AbstractMetadataConsumerPtr = QSharedPointer<AbstractMetadataConsumer>;

} // namespace media
} // namespace nx
