#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/maintenance_manager.h"

namespace nx::cloud::db::client {

class MaintenanceManager:
    public api::MaintenanceManager,
    public AsyncRequestsExecutor
{
public:
    MaintenanceManager(network::cloud::CloudModuleUrlFetcher* const cloudModuleEndpointFetcher);

    virtual void getConnectionsFromVms(
        std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler) override;

    virtual void getStatistics(
        std::function<void(api::ResultCode, api::Statistics)> completionHandler) override;
};

} // namespace nx::cloud::db::client
