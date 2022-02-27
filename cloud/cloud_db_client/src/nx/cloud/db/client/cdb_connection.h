// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <include/nx/cloud/db/api/connection.h>

#include "account_manager.h"
#include "auth_provider.h"
#include "async_http_requests_executor.h"
#include "maintenance_manager.h"
#include "system_manager.h"
#include "oauth_manager.h"
#include "two_factor_auth_manager.h"

namespace nx::cloud::db::client {

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
    //! Implemetation of api::Connection::OauthManager
    virtual api::OauthManager* oauthManager() override;
    //! Implemetation of api::Connection::TwoFactorAuthManager
    virtual api::TwoFactorAuthManager* twoFactorAuthManager() override;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    //!Implemetation of api::Connection::setCredentials
    virtual void setCredentials(nx::network::http::Credentials credentials) override;

    //!Implemetation of api::Connection::setProxyVia
    virtual void setProxyVia(
        const std::string& proxyHost,
        std::uint16_t proxyPort,
        nx::network::http::Credentials credentials,
        nx::network::ssl::AdapterFunc adapterFunc) override;

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
    std::unique_ptr<OauthManager> m_oauthManager;
    std::unique_ptr<TwoFactorAuthManager> m_twoFactorAuthManager;
    AsyncRequestsExecutor m_requestExecutor;
};

} // namespace nx::cloud::db::client
