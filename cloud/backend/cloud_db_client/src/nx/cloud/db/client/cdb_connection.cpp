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

void Connection::bindToAioThread(
    nx::network::aio::AbstractAioThread* aioThread)
{
    m_requestExecutor.bindToAioThread(aioThread);

    m_accountManager->bindToAioThread(aioThread);
    m_systemManager->bindToAioThread(aioThread);
    m_authProvider->bindToAioThread(aioThread);
    m_maintenanceManager->bindToAioThread(aioThread);
}

void Connection::setCredentials(
    const std::string& login,
    const std::string& password)
{
    m_accountManager->setCredentials(login, password);
    m_systemManager->setCredentials(login, password);
    m_authProvider->setCredentials(login, password);
}

void Connection::setProxyCredentials(
    const std::string& login,
    const std::string& password)
{
    m_accountManager->setProxyCredentials(login, password);
    m_systemManager->setProxyCredentials(login, password);
    m_authProvider->setProxyCredentials(login, password);
}

void Connection::setProxyVia(
    const std::string& proxyHost,
    std::uint16_t proxyPort)
{
    const nx::network::SocketAddress proxyEndpoint(proxyHost.c_str(), proxyPort);
    m_accountManager->setProxyVia(proxyEndpoint, /*isSecure*/ true);
    m_systemManager->setProxyVia(proxyEndpoint, /*isSecure*/ true);
    m_authProvider->setProxyVia(proxyEndpoint, /*isSecure*/ true);
}

void Connection::setRequestTimeout(std::chrono::milliseconds timeout)
{
    m_accountManager->setRequestTimeout(timeout);
    m_systemManager->setRequestTimeout(timeout);
    m_authProvider->setRequestTimeout(timeout);
    m_maintenanceManager->setRequestTimeout(timeout);
    m_requestExecutor.setRequestTimeout(timeout);
}

std::chrono::milliseconds Connection::requestTimeout() const
{
    return m_requestExecutor.requestTimeout();
}

void Connection::ping(
    std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler)
{
    m_requestExecutor.executeRequest(
        kPingPath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::ModuleInfo()));
}

} // namespace nx::cloud::db::client
