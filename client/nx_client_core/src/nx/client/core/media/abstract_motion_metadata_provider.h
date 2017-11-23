#pragma once

#include <memory>

#include <QtCore/QScopedPointer>

struct QnMetaDataV1;

namespace nx {
namespace client {
namespace core {

typedef std::shared_ptr<QnMetaDataV1> MetaDataV1Ptr;

class AbstractMotionMetadataProvider
{
public:
    AbstractMotionMetadataProvider();
    virtual ~AbstractMotionMetadataProvider();

    virtual MetaDataV1Ptr metadata(const qint64 timestamp, int channel) = 0;
};

} // namespace core
} // namespace client
} // namespace nx
