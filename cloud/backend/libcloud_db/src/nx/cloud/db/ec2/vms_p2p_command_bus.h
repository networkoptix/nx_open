#pragma once

#include <nx/sql/query_context.h>

#include <nx/vms/api/data/resource_data.h>

namespace nx::clusterdb::engine { class SynchronizationEngine; }

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
    VmsP2pCommandBus(clusterdb::engine::SynchronizationEngine* synchronizationEngine);

    virtual nx::sql::DBResult saveResourceAttribute(
        nx::sql::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) override;

private:
    clusterdb::engine::SynchronizationEngine* m_synchronizationEngine;
};

} // namespace ec2
} // namespace nx::cloud::db
