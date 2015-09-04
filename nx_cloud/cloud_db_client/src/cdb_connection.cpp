/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "cdb_connection.h"


namespace nx {
namespace cdb {
namespace cl {

Connection::Connection(
    const std::string& login,
    const std::string& password)
:
    m_login(login),
    m_password(password)
{
}

api::AccountManager* Connection::getAccountManager()
{
    return m_accountManager.get();
}

api::SystemManager* Connection::getSystemManager()
{
    return m_systemManager.get();
}

void Connection::setCredentials(
    const std::string& /*login*/,
    const std::string& /*password*/)
{
    //TODO #ak
}

void Connection::ping(std::function<void(api::ResultCode)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented);
}

}   //cl
}   //cdb
}   //nx
