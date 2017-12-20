#pragma once

#include <QtCore/QSharedPointer>

#include "abstract_motion_metadata_provider.h"
#include "abstract_metadata_consumer_owner.h"

namespace nx {
namespace client {
namespace core {

class ConsumingMotionMetadataProvider:
    public AbstractMotionMetadataProvider,
    public AbstractMetadataConsumerOwner
{
    using base_type = AbstractMotionMetadataProvider;

public:
    ConsumingMotionMetadataProvider();

    virtual MetaDataV1Ptr metadata(const qint64 timestamp, int channel) const override;

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace core
} // namespace client
} // namespace nx
