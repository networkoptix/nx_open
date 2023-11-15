// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <include/nx/cloud/db/api/connection.h>

#include "account_manager.h"
#include "async_http_requests_executor.h"
#include "auth_provider.h"
#include "batch_user_processing_manager.h"
#include "maintenance_manager.h"
#include "oauth_manager.h"
#include "organization_manager.h"
#include "system_manager.h"
#include "two_factor_auth_manager.h"

namespace nx::cloud::db::client {

class Connection:
    public api::Connection
{
public:
    Connection(network::cloud::CloudModuleUrlFetcher* const endPointFetcher);

    virtual api::AccountManager* accountManager() override;
    virtual api::SystemManager* systemManager() override;
    virtual api::OrganizationManager* organizationManager() override;
    virtual api::AuthProvider* authProvider() override;
    virtual api::MaintenanceManager* maintenanceManager() override;
    virtual api::OauthManager* oauthManager() override;
    virtual api::TwoFactorAuthManager* twoFactorAuthManager() override;

    virtual api::BatchUserProcessingManager* batchUserProcessingManager() override;

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override;

    virtual void setCredentials(nx::network::http::Credentials credentials) override;

    virtual void setProxyVia(
        const std::string& proxyHost,
        std::uint16_t proxyPort,
        nx::network::http::Credentials credentials,
        nx::network::ssl::AdapterFunc adapterFunc) override;

    virtual void setRequestTimeout(std::chrono::milliseconds) override;
    virtual std::chrono::milliseconds requestTimeout() const override;

    virtual void setAdditionalHeaders(nx::network::http::HttpHeaders headers) override;

    virtual void ping(
        std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler) override;

private:
    AsyncRequestsExecutor m_requestExecutor;
    AccountManager m_accountManager;
    SystemManager m_systemManager;
    OrganizationManager m_organizationManager;
    AuthProvider m_authProvider;
    MaintenanceManager m_maintenanceManager;
    OauthManager m_oauthManager;
    TwoFactorAuthManager m_twoFactorAuthManager;
    BatchUserProcessingManager m_batchUserProcessingManager;
};

} // namespace nx::cloud::db::client
