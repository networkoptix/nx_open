#pragma once

#include <include/nx/cloud/cdb/api/connection.h>

#include "account_manager.h"
#include "auth_provider.h"
#include "async_http_requests_executor.h"
#include "maintenance_manager.h"
#include "system_manager.h"

namespace nx {
namespace cdb {
namespace client {

class Connection:
    public api::Connection
{
public:
    Connection(
        network::cloud::CloudModuleUrlFetcher* const endPointFetcher);

    //!Implemetation of api::Connection::getAccountManager
    virtual api::AccountManager* accountManager() override;
    //!Implemetation of api::Connection::getAccountManager
    virtual api::SystemManager* systemManager() override;
    //!Implemetation of api::Connection::authProvider
    virtual api::AuthProvider* authProvider() override;
    //!Implemetation of api::Connection::maintenanceManager
    virtual api::MaintenanceManager* maintenanceManager() override;

    //!Implemetation of api::Connection::setCredentials
    virtual void setCredentials(
        const std::string& login,
        const std::string& password) override;

    //!Implemetation of api::Connection::setProxyCredentials
    virtual void setProxyCredentials(
        const std::string& login,
        const std::string& password) override;

    virtual void setProxyVia(
        const std::string& proxyHost,
        std::uint16_t proxyPort) override;

    virtual void setRequestTimeout(std::chrono::milliseconds) override;
    virtual std::chrono::milliseconds requestTimeout() const override;

    //!Implemetation of api::Connection::ping
    virtual void ping(
        std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler) override;

private:
    std::unique_ptr<AccountManager> m_accountManager;
    std::unique_ptr<SystemManager> m_systemManager;
    std::unique_ptr<AuthProvider> m_authProvider;
    std::unique_ptr<MaintenanceManager> m_maintenanceManager;
    AsyncRequestsExecutor m_requestExecutor;
};

} // namespace client
} // namespace cdb
} // namespace nx
