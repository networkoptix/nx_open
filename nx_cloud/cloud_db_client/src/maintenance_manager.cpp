#include "maintenance_manager.h"

#include "cdb_request_path.h"
#include "data/maintenance_data.h"

namespace nx {
namespace cdb {
namespace client {

MaintenanceManager::MaintenanceManager(
    network::cloud::CloudModuleUrlFetcher* const cloudModuleEndpointFetcher)
    :
    AsyncRequestsExecutor(cloudModuleEndpointFetcher)
{
}

void MaintenanceManager::getConnectionsFromVms(
    std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler)
{
    executeRequest(
        kMaintenanceGetVmsConnections,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::VmsConnectionDataList()));
}

void MaintenanceManager::getStatistics(
    std::function<void(api::ResultCode, api::Statistics)> completionHandler)
{
    executeRequest(
        kMaintenanceGetStatistics,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::Statistics()));
}

} // namespace client
} // namespace cdb
} // namespace nx
