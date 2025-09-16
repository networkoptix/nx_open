// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include "abstract_metadata_consumer.h"

namespace nx::analytics {

class NX_VMS_COMMON_API SignalingMetadataConsumer:
    public QObject,
    public AbstractMetadataConsumer
{
    Q_OBJECT

public:
    explicit SignalingMetadataConsumer(MetadataType metadataType, QObject* parent = nullptr);
    virtual ~SignalingMetadataConsumer() override;

signals:
    void metadataReceived(const QnAbstractCompressedMetadataPtr& metadata);

protected:
    virtual void processMetadata(const QnAbstractCompressedMetadataPtr& metadata) override;
};

} // namespace bx::analytics
