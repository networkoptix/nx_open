#pragma once

#include <functional>

#include <cloud_db_client/src/data/maintenance_data.h>
#include <nx/network/aio/timer.h>
#include <utils/common/counter.h>

namespace nx {
namespace cdb {

class AuthorizationInfo;

namespace ec2 {
class ConnectionManager;
} // namespace ec2

class MaintenanceManager
{
public:
    MaintenanceManager(const ec2::ConnectionManager& connectionManager);
    ~MaintenanceManager();

    void getVmsConnections(
        const AuthorizationInfo& authzInfo,
        std::function<void(
            api::ResultCode,
            api::VmsConnectionDataList)> completionHandler);

private:
    const ec2::ConnectionManager& m_connectionManager;
    nx::network::aio::Timer m_timer;
    QnCounter m_startedAsyncCallsCounter;
};

} // namespace cdb
} // namespace nx
