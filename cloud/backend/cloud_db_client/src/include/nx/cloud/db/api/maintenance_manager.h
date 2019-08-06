#pragma once

#include <functional>
#include <string>
#include <vector>

#include "result_code.h"

namespace nx::cloud::db::api {

class VmsConnectionData
{
public:
    std::string systemId;
    std::string mediaserverEndpoint;
};

class VmsConnectionDataList
{
public:
    std::vector<VmsConnectionData> connections;
};

class Statistics
{
public:
    int onlineServerCount;

    Statistics():
        onlineServerCount(0)
    {
    }
};

/**
 * Maintenance manager is for accessing cloud internal data for maintenance/debug purposes.
 * \note Available only in debug environment.
 */
class MaintenanceManager
{
public:
    virtual ~MaintenanceManager() = default;

    /**
     * Provides list of connections mediaservers have established to the cloud.
     */
    virtual void getConnectionsFromVms(
        std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler) = 0;

    virtual void getStatistics(
        std::function<void(api::ResultCode, api::Statistics)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
