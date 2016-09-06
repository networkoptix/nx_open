#include "maintenance_manager.h"

#include "../ec2/connection_manager.h"

namespace nx {
namespace cdb {

MaintenanceManager::MaintenanceManager(
    const ec2::ConnectionManager& connectionManager)
    :
    m_connectionManager(connectionManager)
{
}

MaintenanceManager::~MaintenanceManager()
{
    m_startedAsyncCallsCounter.wait();
    m_timer.pleaseStopSync();
}

void MaintenanceManager::getVmsConnections(
    const AuthorizationInfo& /*authzInfo*/,
    std::function<void(
        api::ResultCode,
        api::VmsConnectionDataList)> completionHandler)
{
    m_timer.post(
        [this,
            lock = m_startedAsyncCallsCounter.getScopedIncrement(),
            completionHandler = std::move(completionHandler)]()
        {
            completionHandler(
                api::ResultCode::ok,
                m_connectionManager.getVmsConnections());
        });
}

} // maintenance cdb
} // maintenance nx
