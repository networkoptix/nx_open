#pragma once

#include <QtCore/QObject>

#include "abstract_metadata_consumer.h"

namespace nx {
namespace media {

class SignalingMetadataConsumer:
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

} // namespace media
} // namespace nx
