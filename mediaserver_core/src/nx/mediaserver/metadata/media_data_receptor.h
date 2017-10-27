#pragma once

#include "manager_pool.h"

namespace nx {
namespace mediaserver {
namespace metadata {

class ResourceMetadataContext;

struct DataReceptor: public QnAbstractDataReceptor
{
public:
    DataReceptor(const ResourceMetadataContext* context);

    virtual bool canAcceptData() const override { return true;  }
    virtual void putData(const QnAbstractDataPacketPtr& data) override;
private:
    const ResourceMetadataContext* m_context;
};

} // namespace metadata
} // namespace mediaserver
} // namespace nx
