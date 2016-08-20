/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "cdb_connection.h"

#include "cdb_request_path.h"
#include "data/module_info.h"


namespace nx {
namespace cdb {
namespace cl {

Connection::Connection(
    network::cloud::CloudModuleEndPointFetcher* const endPointFetcher)
    :
    AsyncRequestsExecutor(endPointFetcher)
{
    m_accountManager = std::make_unique<AccountManager>(endPointFetcher);
    m_systemManager = std::make_unique<SystemManager>(endPointFetcher);
    m_authProvider = std::make_unique<AuthProvider>(endPointFetcher);

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

void Connection::setProxyVia(const SocketAddress& proxyEndpoint)
{
    m_accountManager->setProxyVia(proxyEndpoint);
    m_systemManager->setProxyVia(proxyEndpoint);
    m_authProvider->setProxyVia(proxyEndpoint);
}

void Connection::ping(
    std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler)
{
    executeRequest(
        kPingPath,
        completionHandler,
        std::bind(completionHandler, std::placeholders::_1, api::ModuleInfo()));
}

}   //cl
}   //cdb
}   //nx
