// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "signaling_metadata_consumer.h"

namespace nx::analytics {

SignalingMetadataConsumer::SignalingMetadataConsumer(MetadataType metadataType, QObject* parent):
    QObject(parent),
    AbstractMetadataConsumer(metadataType)
{
}

SignalingMetadataConsumer::~SignalingMetadataConsumer()
{
}

void SignalingMetadataConsumer::processMetadata(const QnConstAbstractCompressedMetadataPtr& metadata)
{
    emit metadataReceived(metadata);
}

} // namespace nx::analytics
