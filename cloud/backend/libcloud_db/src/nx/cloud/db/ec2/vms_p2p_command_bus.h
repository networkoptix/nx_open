#pragma once

#include <nx/sql/query_context.h>

#include <nx/vms/api/data/resource_data.h>

namespace nx::clusterdb::engine { class SyncronizationEngine; }

namespace nx::cloud::db {
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
    VmsP2pCommandBus(clusterdb::engine::SyncronizationEngine* syncronizationEngine);

    virtual nx::sql::DBResult saveResourceAttribute(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) override;

private:
    clusterdb::engine::SyncronizationEngine* m_syncronizationEngine;
};

} // namespace ec2
} // namespace nx::cloud::db
