#pragma once

#include <QtCore/QSharedPointer>

#include "abstract_motion_metadata_provider.h"

namespace nx {

namespace media {

class AbstractMetadataConsumer;

} // namespace media

namespace client {
namespace core {

class ConsumingMotionMetadataProvider: public AbstractMotionMetadataProvider
{
    using base_type = AbstractMotionMetadataProvider;

public:
    ConsumingMotionMetadataProvider();

    virtual MetaDataV1Ptr metadata(const qint64 timestamp, int channel) override;

    QSharedPointer<media::AbstractMetadataConsumer> metadataConsumer() const;

private:
    class Private;
    QScopedPointer<Private> const d;
};

} // namespace core
} // namespace client
} // namespace nx
