/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_CL_CDB_CONNECTION_H
#define NX_CDB_CL_CDB_CONNECTION_H

#include <include/cdb/connection.h>

#include "account_manager.h"
#include "system_manager.h"


namespace nx {
namespace cdb {
namespace cl {

class Connection
:
    public api::Connection
{
public:
    Connection(
        const std::string& login,
        const std::string& password );

    //!Implemetation of api::Connection::getAccountManager
    virtual api::AccountManager* getAccountManager() override;
    //!Implemetation of api::Connection::getAccountManager
    virtual api::SystemManager* getSystemManager() override;

    //!Implemetation of api::Connection::setCredentials
    virtual void setCredentials(
        const std::string& login,
        const std::string& password) override;
    //!Implemetation of api::Connection::ping
    virtual void ping(std::function<void(api::ResultCode)> completionHandler) override;

private:
    std::string m_login;
    std::string m_password;
    std::unique_ptr<AccountManager> m_accountManager;
    std::unique_ptr<SystemManager> m_systemManager;
};

}   //cl
}   //cdb
}   //nx

#endif  //NX_CDB_CL_CDB_CONNECTION_H
