#pragma once

#include <nx/utils/db/query_context.h>

#include <nx/vms/api/data/resource_data.h>

namespace nx {
namespace cdb {
namespace ec2 {

class AbstractVmsP2pCommandBus
{
public:
    virtual ~AbstractVmsP2pCommandBus() = default;

    virtual nx::utils::db::DBResult saveResourceAttribute(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) = 0;
};

//-------------------------------------------------------------------------------------------------

class SyncronizationEngine;

class VmsP2pCommandBus:
    public AbstractVmsP2pCommandBus
{
public:
    VmsP2pCommandBus(SyncronizationEngine* syncronizationEngine);

    virtual nx::utils::db::DBResult saveResourceAttribute(
        nx::utils::db::QueryContext* queryContext,
        const std::string& systemId,
        nx::vms::api::ResourceParamWithRefData data) override;

private:
    SyncronizationEngine* m_syncronizationEngine;
};

} // namespace ec2
} // namespace cdb
} // namespace nx
