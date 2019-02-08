#include "vms_p2p_command_bus.h"

#include <nx/clusterdb/engine/synchronization_engine.h>
#include <nx/cloud/db/ec2/vms_command_descriptor.h>

namespace nx::cloud::db {
namespace ec2 {

VmsP2pCommandBus::VmsP2pCommandBus(
    clusterdb::engine::SyncronizationEngine* syncronizationEngine)
    :
    m_syncronizationEngine(syncronizationEngine)
{
}

nx::sql::DBResult VmsP2pCommandBus::saveResourceAttribute(
    nx::sql::QueryContext* queryContext,
    const std::string& systemId,
    nx::vms::api::ResourceParamWithRefData data)
{
    return m_syncronizationEngine->transactionLog().generateTransactionAndSaveToLog
        <command::SetResourceParam>(
            queryContext,
            systemId.c_str(),
            std::move(data));
}

} // namespace ec2
} // namespace nx::cloud::db
