// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "maintenance_manager.h"

#include <nx/network/http/rest/http_rest_client.h>

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

void MaintenanceManager::getAllAccountLocks(
    std::function<void(api::ResultCode, api::AccountLockList)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::AccountLockList>(
        nx::network::http::Method::get,
        kMaintenanceSecurityLocks,
        {},
        std::move(completionHandler));
}

void MaintenanceManager::getLockedHosts(
    std::function<void(api::ResultCode, api::AccountLockList)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::AccountLockList>(
        nx::network::http::Method::get,
        kMaintenanceSecurityLocksHosts,
        {},
        std::move(completionHandler));
}

void MaintenanceManager::isHostLocked(
    std::string ipAddress,
    std::function<void(api::ResultCode, bool)> completionHandler)
{
    using namespace nx::network::http;
    m_requestsExecutor->makeAsyncCall<bool>(
        Method::get,
        rest::substituteParameters(kMaintenanceSecurityLocksHostsIp, {ipAddress}),
        {},
        std::move(completionHandler));
}

void MaintenanceManager::getLockedUsers(
    std::function<void(api::ResultCode, api::AccountLockList)> completionHandler)
{
    m_requestsExecutor->makeAsyncCall<api::AccountLockList>(
        nx::network::http::Method::get,
        kMaintenanceSecurityLocksUsers,
        {},
        std::move(completionHandler));
}

void MaintenanceManager::isUserLocked(
    std::string username,
    std::function<void(api::ResultCode, bool)> completionHandler)
{
    using namespace nx::network::http;
    m_requestsExecutor->makeAsyncCall<bool>(
        Method::get,
        rest::substituteParameters(kMaintenanceSecurityLocksUsersUsername, {username}),
        {},
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
