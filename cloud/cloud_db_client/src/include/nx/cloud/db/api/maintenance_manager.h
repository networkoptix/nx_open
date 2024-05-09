// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "maintenance_data.h"
#include "result_code.h"

namespace nx::cloud::db::api {

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

    /**
     * provides a list of all account locks, both host locks and user locks
     */
    virtual void getAllAccountLocks(
        std::function<void(api::ResultCode, api::AccountLockList)> completionHandler) = 0;

    /**
     * Provides a list of account locks with LockType::host.
     */
    virtual void getLockedHosts(
        std::function<void(api::ResultCode, api::AccountLockList)> completionHandler) = 0;

    virtual void isHostLocked(
        std::string ipAddress,
        std::function<void (api::ResultCode, bool)> completionHandler) = 0;

    /**
     * Provides a list of account locks with LockType::user
     */
    virtual void getLockedUsers(
        std::function<void(api::ResultCode, api::AccountLockList)> completionHandler) = 0;

    virtual void isUserLocked(
        std::string username,
        std::function<void (api::ResultCode, bool)> completionHandler) = 0;
};

} // namespace nx::cloud::db::api
