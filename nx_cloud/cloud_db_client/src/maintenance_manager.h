#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/cdb/maintenance_manager.h"

namespace nx {
namespace cdb {
namespace cl {

class MaintenanceManager:
    public api::MaintenanceManager,
    public AsyncRequestsExecutor
{
public:
    MaintenanceManager(network::cloud::CloudModuleEndPointFetcher* const cloudModuleEndpointFetcher);

    virtual void getConnectionsFromVms(
        std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler) override;
};

} // namespace cl
} // namespace cdb
} // namespace nx
