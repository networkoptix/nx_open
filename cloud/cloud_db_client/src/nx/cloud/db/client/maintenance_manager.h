// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include "async_http_requests_executor.h"
#include "include/nx/cloud/db/api/maintenance_manager.h"

namespace nx::cloud::db::client {

class MaintenanceManager:
    public api::MaintenanceManager
{
public:
    MaintenanceManager(ApiRequestsExecutor* requestsExecutor);

    virtual void getConnectionsFromVms(
        std::function<void(api::ResultCode, api::VmsConnectionDataList)> completionHandler) override;

    virtual void getStatistics(
        std::function<void(api::ResultCode, api::Statistics)> completionHandler) override;

    virtual void getAllAccountLocks(
        std::function<void(api::ResultCode, api::AccountLockList)> completionHandler) override;

    virtual void getLockedHosts(
        std::function<void(api::ResultCode, api::AccountLockList)> completionHandler) override;

    virtual void isHostLocked(
        std::string ipAddress,
        std::function<void (api::ResultCode, bool)> completionHandler) override;

    virtual void getLockedUsers(
        std::function<void(api::ResultCode, api::AccountLockList)> completionHandler) override;

    virtual void isUserLocked(
        std::string username,
        std::function<void (api::ResultCode, bool)> completionHandler) override;

private:
    ApiRequestsExecutor* m_requestsExecutor = nullptr;
};

} // namespace nx::cloud::db::client
