// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_metadata_consumer.h"

namespace nx {
namespace media {

AbstractMetadataConsumer::~AbstractMetadataConsumer()
{
}

MetadataType AbstractMetadataConsumer::metadataType() const
{
    return m_metadataType;
}

AbstractMetadataConsumer::AbstractMetadataConsumer(MetadataType metadataType):
    m_metadataType(metadataType)
{
}

} // namespace media
} // namespace nx
