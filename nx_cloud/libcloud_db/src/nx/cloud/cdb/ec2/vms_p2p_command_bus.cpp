#include "vms_p2p_command_bus.h"

#include <nx/data_sync_engine/synchronization_engine.h>

namespace nx {
namespace cdb {
namespace ec2 {

VmsP2pCommandBus::VmsP2pCommandBus(SyncronizationEngine* syncronizationEngine):
    m_syncronizationEngine(syncronizationEngine)
{
}

nx::utils::db::DBResult VmsP2pCommandBus::saveResourceAttribute(
    nx::utils::db::QueryContext* queryContext,
    const std::string& systemId,
    ::ec2::ApiResourceParamWithRefData data)
{
    return m_syncronizationEngine->transactionLog().generateTransactionAndSaveToLog(
        queryContext,
        systemId.c_str(),
        ::ec2::ApiCommand::setResourceParam,
        std::move(data));
}

} // namespace ec2
} // namespace cdb
} // namespace nx
