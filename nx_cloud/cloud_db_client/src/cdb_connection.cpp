/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "cdb_connection.h"


namespace nx {
namespace cdb {
namespace cl {

Connection::Connection(
    CloudModuleEndPointFetcher* const endPointFetcher,
    const std::string& login,
    const std::string& password)
{
    m_accountManager = std::make_unique<AccountManager>(endPointFetcher);
    m_systemManager = std::make_unique<SystemManager>(endPointFetcher);

    setCredentials(login, password);
}

api::AccountManager* Connection::accountManager()
{
    return m_accountManager.get();
}

api::SystemManager* Connection::systemManager()
{
    return m_systemManager.get();
}

void Connection::setCredentials(
    const std::string& login,
    const std::string& password)
{
    m_accountManager->setCredentials(login, password);
    m_systemManager->setCredentials(login, password);
}

void Connection::ping(std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented);
}

}   //cl
}   //cdb
}   //nx
