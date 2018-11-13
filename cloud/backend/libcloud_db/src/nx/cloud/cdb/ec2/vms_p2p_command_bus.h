#pragma once

#include <nx/sql/query_context.h>

#include <nx/vms/api/data/resource_data.h>

namespace nx { namespace data_sync_engine { class SyncronizationEngine; } }

namespace nx::cdb {
namespace ec2 {

class AbstractVmsP2pCommandBus
{
public:
    virtual ~AbstractVmsP2pCommandBus() = default;

    virtual nx::sql::DBResult saveResourceAttribute(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) = 0;
};

//-------------------------------------------------------------------------------------------------

class VmsP2pCommandBus:
    public AbstractVmsP2pCommandBus
{
public:
    VmsP2pCommandBus(data_sync_engine::SyncronizationEngine* syncronizationEngine);

    virtual nx::sql::DBResult saveResourceAttribute(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) override;

private:
    data_sync_engine::SyncronizationEngine* m_syncronizationEngine;
};

} // namespace ec2
} // namespace nx::cdb
