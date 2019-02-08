#pragma once

#include <QtCore/QSharedPointer>

#include "abstract_motion_metadata_provider.h"
#include "abstract_metadata_consumer_owner.h"

namespace nx::vms::client::core {

class ConsumingMotionMetadataProvider:
    public AbstractMotionMetadataProvider,
    public AbstractMetadataConsumerOwner
{
    using base_type = AbstractMotionMetadataProvider;

public:
    ConsumingMotionMetadataProvider();
    virtual ~ConsumingMotionMetadataProvider() override;

    virtual MetaDataV1Ptr metadata(
        const std::chrono::microseconds timestamp, int channel) const override;

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const override;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace nx::vms::client::core
