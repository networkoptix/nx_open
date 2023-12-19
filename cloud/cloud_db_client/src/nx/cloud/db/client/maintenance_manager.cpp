// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "maintenance_manager.h"

#include "cdb_request_path.h"
#include "data/maintenance_data.h"

namespace nx::cloud::db::client {

MaintenanceManager::MaintenanceManager(ApiRequestsExecutor* requestsExecutor):
    m_requestsExecutor(requestsExecutor)
{
}

void MaintenanceManager::getConnectionsFromVms(
    std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::VmsConnectionDataList>(
        nx::network::http::Method::get,
        kMaintenanceGetVmsConnections,
        {}, //query
        std::move(completionHandler));
}

void MaintenanceManager::getStatistics(
    std::function<void(api::ResultCode, api::Statistics)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::Statistics>(
        nx::network::http::Method::get,
        kMaintenanceGetStatistics,
        {}, //query
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
