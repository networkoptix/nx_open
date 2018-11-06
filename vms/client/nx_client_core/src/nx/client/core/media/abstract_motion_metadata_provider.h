#pragma once

#include <memory>

#include <QtCore/QScopedPointer>

struct QnMetaDataV1;

namespace nx::vms::client::core {

typedef std::shared_ptr<QnMetaDataV1> MetaDataV1Ptr;

class AbstractMotionMetadataProvider
{
public:
    AbstractMotionMetadataProvider();
    virtual ~AbstractMotionMetadataProvider();

    virtual MetaDataV1Ptr metadata(const qint64 timestamp, int channel) const = 0;
};

} // namespace nx::vms::client::core
