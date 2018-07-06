#pragma once

#include <QtCore/QSharedPointer>

namespace nx {

namespace media {

class AbstractMetadataConsumer;

} // namespace media

namespace client {
namespace core {

class AbstractMetadataConsumerOwner
{
public:
    virtual ~AbstractMetadataConsumerOwner() {}

    virtual QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const = 0;
};

} // namespace core
} // namespace client
} // namespace nx
