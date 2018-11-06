#pragma once

#include <QtCore/QSharedPointer>

namespace nx::media { class AbstractMetadataConsumer;}

namespace nx::vms::client::core {

class AbstractMetadataConsumerOwner
{
public:
    virtual ~AbstractMetadataConsumerOwner() {}

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const = 0;
};

} // namespace nx::vms::client::core
