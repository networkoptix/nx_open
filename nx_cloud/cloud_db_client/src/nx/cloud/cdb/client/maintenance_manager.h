#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/cdb/api/maintenance_manager.h"

namespace nx {
namespace cdb {
namespace client {

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

} // namespace client
} // namespace cdb
} // namespace nx
