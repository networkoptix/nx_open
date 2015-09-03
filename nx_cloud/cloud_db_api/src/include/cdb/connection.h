/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_CONNECTION_H
#define NX_CDB_API_CONNECTION_H

#include <functional>
#include <memory>
#include <string>

#include "account_manager.h"
#include "result_code.h"
#include "system_manager.h"


namespace nx {
namespace cdb {
/*!
    Contains classes and methods for manipulating cloud_db data.
    Many methods accept \a completionHandler a an argument. 
    This is functor that is called on operation completion/failure 
    and reports \a ResultCode and output data (if applicable).

    Generally, some methods can be forbidden for credentials.
    E.g., if credentials are system credentials, \a api::AccountManager::getAccount is forbidden
*/
namespace api {

class Connection
{
public:
    virtual const std::unique_ptr<AccountManager>& getAccountManager() const = 0;
    virtual const std::unique_ptr<SystemManager>& getSystemManager() const = 0;

    //!Set other credentials if they were changed
    /*!
        This method does not try to connect to cloud_db check credentials
    */
    virtual void setCredentials(
        const std::string& login,
        const std::string& password) = 0;
    //!Pings cloud_db with current creentials
    virtual void ping(std::function<void(ResultCode)> completionHandler) = 0;
};

//!
class ConnectionFactory
{
public:
    //!Connects to cloud_db to check user credentials
    virtual void connect(
        const std::string& login,
        const std::string& password,
        std::function<void(ResultCode, std::unique_ptr<Connection>)> completionHandler) = 0;
};

ConnectionFactory* createConnectionFactory();
void destroyConnectionFactory(ConnectionFactory* factory);

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_CONNECTION_H
