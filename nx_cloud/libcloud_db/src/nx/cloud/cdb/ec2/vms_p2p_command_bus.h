#pragma once

#include <utils/db/query_context.h>

#include <nx_ec/data/api_resource_data.h>
#include <transaction/transaction.h>

namespace nx {
namespace cdb {
namespace ec2 {

class AbstractVmsP2pCommandBus
{
public:
    virtual ~AbstractVmsP2pCommandBus() = default;

    virtual nx::db::DBResult saveResourceAttribute(
        nx::db::QueryContext* queryContext,
        const std::string& systemId,
        ::ec2::ApiResourceParamWithRefData data) = 0;
};

//-------------------------------------------------------------------------------------------------

class SyncronizationEngine;

class VmsP2pCommandBus:
    public AbstractVmsP2pCommandBus
{
public:
    VmsP2pCommandBus(SyncronizationEngine* syncronizationEngine);

    virtual nx::db::DBResult saveResourceAttribute(
        nx::db::QueryContext* queryContext,
        const std::string& systemId,
        ::ec2::ApiResourceParamWithRefData data) override;

private:
    SyncronizationEngine* m_syncronizationEngine;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
