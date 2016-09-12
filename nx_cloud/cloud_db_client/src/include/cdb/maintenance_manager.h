#pragma once

#include <functional>
#include <string>

#include "result_code.h"

namespace nx {
namespace cdb {
namespace api {

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
};

} // namespace api
} // namespace cdb
} // namespace nx
