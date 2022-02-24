// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cdb_connection.h"

#include <nx/network/socket_common.h>

#include "cdb_request_path.h"
#include "data/module_info.h"

namespace nx::cloud::db::client {

Connection::Connection(
    network::cloud::CloudModuleUrlFetcher* const endPointFetcher)
    :
    m_requestExecutor(endPointFetcher)
{
    m_accountManager = std::make_unique<AccountManager>(endPointFetcher);
    m_systemManager = std::make_unique<SystemManager>(endPointFetcher);
    m_authProvider = std::make_unique<AuthProvider>(endPointFetcher);
    m_maintenanceManager = std::make_unique<MaintenanceManager>(endPointFetcher);
    m_oauthManager = std::make_unique<OauthManager>(endPointFetcher);
    m_twoFactorAuthManager = std::make_unique<TwoFactorAuthManager>(endPointFetcher);

    bindToAioThread(m_requestExecutor.getAioThread());

    setRequestTimeout(m_requestExecutor.requestTimeout());
}

api::AccountManager* Connection::accountManager()
{
    return m_accountManager.get();
}

api::SystemManager* Connection::systemManager()
{
    return m_systemManager.get();
}

api::AuthProvider* Connection::authProvider()
{
    return m_authProvider.get();
}

api::MaintenanceManager* Connection::maintenanceManager()
{
    return m_maintenanceManager.get();
}

api::OauthManager* Connection::oauthManager()
{
    return m_oauthManager.get();
}

api::TwoFactorAuthManager* Connection::twoFactorAuthManager()
{
    return m_twoFactorAuthManager.get();
}


void Connection::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    m_requestExecutor.bindToAioThread(aioThread);

    m_accountManager->bindToAioThread(aioThread);
    m_systemManager->bindToAioThread(aioThread);
    m_authProvider->bindToAioThread(aioThread);
    m_maintenanceManager->bindToAioThread(aioThread);
    m_oauthManager->bindToAioThread(aioThread);
    m_twoFactorAuthManager->bindToAioThread(aioThread);
}

void Connection::setCredentials(nx::network::http::Credentials credentials)
{
    // TODO: #akolesnikov refactor: all managers should receive all needed data automatically,
    // without iterating them here.

    m_accountManager->setCredentials(credentials);
    m_systemManager->setCredentials(credentials);
    m_authProvider->setCredentials(credentials);
    m_maintenanceManager->setCredentials(credentials);
    m_oauthManager->setCredentials(credentials);
    m_twoFactorAuthManager->setCredentials(credentials);
}

void Connection::setProxyVia(
    const std::string& proxyHost,
    std::uint16_t proxyPort,
    nx::network::http::Credentials credentials,
    nx::network::ssl::AdapterFunc adapterFunc)
{
    m_accountManager->setProxyCredentials(credentials);
    m_systemManager->setProxyCredentials(credentials);
    m_authProvider->setProxyCredentials(credentials);
    m_maintenanceManager->setProxyCredentials(credentials);
    m_oauthManager->setProxyCredentials(credentials);
    m_twoFactorAuthManager->setProxyCredentials(credentials);

    const nx::network::SocketAddress proxyEndpoint(proxyHost, proxyPort);
    m_accountManager->setProxyVia(proxyEndpoint, adapterFunc, /*isSecure*/ true);
    m_systemManager->setProxyVia(proxyEndpoint, adapterFunc, /*isSecure*/ true);
    m_authProvider->setProxyVia(proxyEndpoint, adapterFunc, /*isSecure*/ true);
    m_maintenanceManager->setProxyVia(proxyEndpoint, adapterFunc, /*isSecure*/ true);
    m_oauthManager->setProxyVia(proxyEndpoint, adapterFunc, /*isSecure*/ true);
    m_twoFactorAuthManager->setProxyVia(proxyEndpoint, adapterFunc, /*isSecure*/ true);
}

void Connection::setRequestTimeout(std::chrono::milliseconds timeout)
{
    m_accountManager->setRequestTimeout(timeout);
    m_systemManager->setRequestTimeout(timeout);
    m_authProvider->setRequestTimeout(timeout);
    m_maintenanceManager->setRequestTimeout(timeout);
    m_oauthManager->setRequestTimeout(timeout);
    m_twoFactorAuthManager->setRequestTimeout(timeout);
    m_requestExecutor.setRequestTimeout(timeout);
}

std::chrono::milliseconds Connection::requestTimeout() const
{
    return m_requestExecutor.requestTimeout();
}

void Connection::ping(
    std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler)
{
    m_requestExecutor.executeRequest<api::ModuleInfo>(
        kPingPath,
        std::move(completionHandler));
}

} // namespace nx::cloud::db::client
