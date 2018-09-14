#include "vms_p2p_command_bus.h"

#include <nx/data_sync_engine/synchronization_engine.h>
#include <nx/cloud/cdb/ec2/vms_command_descriptor.h>

namespace nx {
namespace cdb {
namespace ec2 {

VmsP2pCommandBus::VmsP2pCommandBus(
    data_sync_engine::SyncronizationEngine* syncronizationEngine)
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
} // namespace cdb
} // namespace nx
