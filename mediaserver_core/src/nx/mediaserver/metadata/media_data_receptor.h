#pragma once

#include "manager_pool.h"

namespace nx {
namespace mediaserver {
namespace metadata {

class ResourceMetadataContext;

struct VideoDataReceptor: public QnAbstractDataReceptor
{
public:
    VideoDataReceptor(const ResourceMetadataContext* context);

    virtual bool canAcceptData() const override { return true;  }
    virtual void putData(const QnAbstractDataPacketPtr& data) override;

    void detachFromContext();
private:
    const ResourceMetadataContext* m_context;
    mutable QnMutex m_mutex;
};
using VideoDataReceptorPtr = QSharedPointer<VideoDataReceptor>;

} // namespace metadata
} // namespace mediaserver
} // namespace nx
