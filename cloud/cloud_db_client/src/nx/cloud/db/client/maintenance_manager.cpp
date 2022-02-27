// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "maintenance_manager.h"

#include "cdb_request_path.h"
#include "data/maintenance_data.h"

namespace nx::cloud::db::client {

MaintenanceManager::MaintenanceManager(
    network::cloud::CloudModuleUrlFetcher* const cloudModuleEndpointFetcher)
    :
    AsyncRequestsExecutor(cloudModuleEndpointFetcher)
{
}

void MaintenanceManager::getConnectionsFromVms(
    std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler)
{
    executeRequest<api::VmsConnectionDataList>(
        kMaintenanceGetVmsConnections,
        std::move(completionHandler));
}

void MaintenanceManager::getStatistics(
    std::function<void(api::ResultCode, api::Statistics)> completionHandler)
{
    executeRequest<api::Statistics>(
        kMaintenanceGetStatistics,
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
